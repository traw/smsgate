#ifndef TARIFF_H
#define TARIFF_H

#define BOOST_MPL_LIMIT_STRING_SIZE 60

#include <string>
#include <list>
#include <vector>
#include <map>
#include <boost/serialization/singleton.hpp>
#include <boost/bind.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/set.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/string.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/logic/tribool.hpp>

#include "MessageClassifier.h"
#include "PGSql.h"

template < class ValueDescr, class DefaultValueT >
class TariffValueSingle {
public:
    typedef std::list< std::string > DescriptionList;

    TariffValueSingle( std::string value = boost::mpl::c_str< DefaultValueT >::value ) {
        boost::mpl::for_each< ValueDescr >( generateDescrList( descrList ) );
        bool value_correct = false;
        boost::mpl::for_each< ValueDescr >( checkValue( value, value_correct ) );
        if ( !value_correct )
            value = boost::mpl::c_str< DefaultValueT >::value;
    }

    const DescriptionList& getDescriptions() { return descrList; }

    template<class Archive>
        void serialize(Archive & ar, const unsigned int) {
            ar & BOOST_SERIALIZATION_NVP( value );
        }

protected:
    DescriptionList descrList;
    std::string value;

private:
    class generateDescrList {
    public:
        generateDescrList( DescriptionList& _descrList ): descrList( _descrList ) {}
        template < class Element >
        void operator() (Element) {
            descrList.push_back( boost::mpl::c_str< Element >::value );
        }
    private:
        DescriptionList& descrList;
    };

    class checkValue {
    public:
        checkValue( std::string _value, bool& _res ): value( _value ), res( _res ) {}
        template < class Element >
        void operator() (Element) {
            if ( value == boost::mpl::c_str< Element >::value )
            res = true;
        }

    private:
        std::string value;
        bool& res;
    };
};

template < class ValueDescr >
class TariffValueMulti {
public:
    typedef std::list< std::string > DescriptionList;
    typedef std::set< std::string > ValuesListT;

    TariffValueMulti( ValuesListT _values = ValuesListT() ) {
        boost::mpl::for_each< ValueDescr >( generateDescrList( descrList ) );
        setValues( _values );
    }

    void setValues( ValuesListT _values ) {
        value.clear();
        for ( ValuesListT::iterator it = _values.begin(); it != _values.end(); it++ ) {
            bool value_correct = false;
            boost::mpl::for_each< ValueDescr >( checkValue( *it, value_correct ) );
            if ( !value_correct )
                continue;
            value.insert( *it );
        }
    }

    ValuesListT getValues( ) { return value; }

    const DescriptionList& getDescriptions() { return descrList; }

    template<class Archive>
        void serialize(Archive & ar, const unsigned int) {
            ar & BOOST_SERIALIZATION_NVP( value );
        }

protected:
    DescriptionList descrList;
    ValuesListT value;

private:
    class generateDescrList {
    public:
        generateDescrList( DescriptionList& _descrList ): descrList( _descrList ) {}
        template < class Element >
        void operator() ( Element ) {
            descrList.push_back( boost::mpl::c_str< Element >::value );
        }
    private:
        DescriptionList& descrList;
    };

    class checkValue {
    public:
        checkValue( std::string _value, bool& _res ): value( _value ), res( _res ) {}
        template < class Element >
        void operator() (Element) {
            if ( value == boost::mpl::c_str< Element >::value )
            res = true;
        }

    private:
        std::string value;
        bool& res;
    };
};

template < class Name, class Storage >
class TariffOption: public Storage {
public:
    static std::string getName() { return boost::mpl::c_str< Name >::value; }

    TariffOption() {}
    TariffOption( std::string src ) {
        std::istringstream ifs( src );
        try {
            boost::archive::xml_iarchive ia( ifs );
            ia >> BOOST_SERIALIZATION_NVP( Storage::value );
        } catch ( ... ) {}
    }

    std::string serialize() {
        std::ostringstream ofs;
        try {
            boost::archive::xml_oarchive oa(ofs);
            oa << BOOST_SERIALIZATION_NVP( Storage::value );
        } catch (...) {}
        return ofs.str();
    }

};

class Tariff {
public:
    static const double INVALID_VALUE = -1.0;
    enum OptionLevel {
        OPT_GLOBAL,
        OPT_COUNTRY,
        OPT_OPERATOR
    };

    typedef TariffOption<
                            boost::mpl::string< 'U','n','p','a','i','d','S','t','a','t','u','s','e','s' >,
                            TariffValueMulti
                            <
                                boost::mpl::vector<
                                    boost::mpl::string< 'R','E','J','E','C','T','E','D' >,
                                    boost::mpl::string< 'U','D','E','L','I','V','E','R','E','D' >,
                                    boost::mpl::string< 'E','X','P','I','R','E','D' >
                                >
                            >
                        > TariffOptionPaidStatuses;

    typedef TariffOption<
                            boost::mpl::string< 'U','n','k','n','o','w','n','P','o','l','i','c','y' >,
                            TariffValueSingle
                            <
                                boost::mpl::vector<
                                    boost::mpl::string< 'M','A','X','I','M','U','M' >,
                                    boost::mpl::string< 'A','V','E','R','A','G','E' >,
                                    boost::mpl::string< 'F','R','E','E' >
                                >,
                                boost::mpl::string< 'F','R','E','E' >
                            >
                        > TariffOptionUnknownPolicy;

    Tariff( );
    Tariff( std::string name );
    Tariff( std::string name, std::string source );

    std::string serialize();

    void addFilterCountry( std::string cname, double price );
    void addFilterCountryOperator( std::string cname, std::string opcode, double price );

    double costs( std::string op );
    double costs( std::string cname, std::string opcode );

    void setName( std::string n ) { tariff.name = n; }
    std::string getName( ) { return tariff.name; };

    struct TariffOperatorInfo {
        std::map< std::string, std::string > options;

        template<class Archive>
            void serialize(Archive & ar, const unsigned int) {
                ar & BOOST_SERIALIZATION_NVP(options);
            }
    };

    struct TariffCountryInfo {
        std::map< std::string, std::string > options;
        std::map< std::string, TariffOperatorInfo > operators;

        template<class Archive>
            void serialize(Archive & ar, const unsigned int) {
                ar & BOOST_SERIALIZATION_NVP(options);
                ar & BOOST_SERIALIZATION_NVP(operators);
            }
    };

    struct TariffInfo {
        std::string name;
        std::map< std::string, std::string > options;
        std::map< std::string, TariffCountryInfo > countries;
        template<class Archive>
            void serialize(Archive & ar, const unsigned int) {
                ar & BOOST_SERIALIZATION_NVP(name);
                ar & BOOST_SERIALIZATION_NVP(options);
                ar & BOOST_SERIALIZATION_NVP(countries);
            }
    };

    void addFilterCountry( std::string cname, float price );
    void addFilterCountryOperator( std::string cname, std::string opcode, float price );
    float costs( sms::OpInfo& op ) const;
    float costs( std::string cname, std::string opcode = "" ) const;

    template<class Archive>
        void serialize(Archive & ar, const unsigned int) {
            ar & BOOST_SERIALIZATION_NVP( tariff );
        }

    template < class Option >
    boost::logic::tribool hasOption() { return hasOption( Option::getName() ); }

    template < class Option >
    boost::logic::tribool hasOption( std::string country ) { return hasOption( Option::getName(), country ); }

    template < class Option >
    boost::logic::tribool hasOption( std::string country, std::string oper ) { return hasOption( Option::getName(), country, oper ); }

    template < class Option >
    Option getOption() { return Option( getOption( Option::getName() ) ); }

    template < class Option >
    Option getOption( std::string country ) { return Option( getOption( Option::getName(), country ) ); }

    template < class Option >
    Option getOption( std::string country, std::string oper ) { return Option( getOption( Option::getName(), country, oper ) ); }

    template < class Option >
    void setOption( Option option ) { setOption( Option::getName(), option.serialize() ); }

    template < class Option >
    void setOption( Option option, std::string country ) { setOption( Option::getName(), country, option.serialize() ); }

    template < class Option >
    void setOption( Option option, std::string country, std::string oper ) { setOption( Option::getName(), country, oper, option.serialize() ); }

    template < class Option >
    void removeOption() { return removeOption( Option::getName() ); }

    template < class Option >
    void removeOption( std::string country ) { return removeOption( Option::getName(), country ); }

    template < class Option >
    void removeOption( std::string country, std::string oper ) { return removeOption( Option::getName(), country, oper ); }


private:
    TariffInfo tariff;

    boost::logic::tribool hasOption( std::string name );
    boost::logic::tribool hasOption( std::string name, std::string country );
    boost::logic::tribool hasOption( std::string name, std::string country, std::string oper );

    std::string getOption( std::string name );
    std::string getOption( std::string name, std::string country );
    std::string getOption( std::string name, std::string country, std::string oper );

    void setOption( std::string name, std::string value );
    void setOption( std::string name, std::string country, std::string value );
    void setOption( std::string name, std::string country, std::string oper, std::string value );

    void removeOption( std::string name );
    void removeOption( std::string name, std::string country );
    void removeOption( std::string name, std::string country, std::string oper );
};

class TariffManager: public boost::serialization::singleton< TariffManager > {
public:
    typedef std::map< std::string, Tariff > TariffMapT;
    typedef std::list< std::string > TariffListT;

    TariffManager();
    ~TariffManager();

    void updateTariffList();
    Tariff loadTariff( std::string name );
    void saveTariff( std::string name, Tariff t );
    void removeTariff( std::string name );
    TariffListT tariffs_list();

private:
    TariffListT tlist;
    TariffMapT tmap;
    int updateTimerID;

    PGSql& db;
};

#endif // TARIFF_H
