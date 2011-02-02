#ifndef MESSAGECLASSIFIER_H
#define MESSAGECLASSIFIER_H

#include <boost/serialization/singleton.hpp>

#include "ConfigManager.h"
#include "PGSql.h"

#include <set>
#include <map>

namespace sms {
    struct OpInfo;

    struct CountryOperatorInfo {
        int mcc;
        int mnc;
        std::string cCode;
        std::string cName;
        std::string cPreffix;
        std::string opCompany;
        std::string opName;
    };


    class MessageClassifier: public boost::serialization::singleton< MessageClassifier > {
    public:

        typedef std::multimap< std::string, OpInfo > DictT;
        typedef std::map< std::string, std::pair< std::string, std::string > > ReplaceT;
        typedef std::map< std::string, std::set< std::string > > CountryOperatorT;

        typedef std::map< int, CountryOperatorInfo > OperatorT;
        typedef std::map< int, OperatorT > CountryOperatorMapT;

        MessageClassifier();

        OpInfo getMsgClass( std::string phone );
        std::string applyReplace( std::string phone );
        CountryOperatorT getCOMap();

        CountryOperatorMapT getCOMap_v2();

    private:
        DictT dict;
        ReplaceT replaces;
        CountryOperatorMapT comap;

        void loadOpcodes();
        void loadReplacesMap();
        void loadCountryOperatorMap();
    };

    struct OpInfo {               
        int countrycode;
        std::string country;
        std::string opcode;
        std::string opname;
        std::string opregion;

        typedef std::map< std::string, std::string > CostMapT;

        OpInfo( int ccode,
                std::string clcode,
                std::string opcode,
                std::string opname,
                std::string opregion);

        OpInfo();
    };

}

#endif // MESSAGECLASSIFIER_H
