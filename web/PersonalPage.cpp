#include "PersonalPage.h"
#include "PartnerManager.h"

using namespace Wt;

PGSql& PersonalPage::db( PGSqlConnPoolStats::get_mutable_instance().getdb() );

WStatPageHeader::WStatPageHeader( PersonalPage* ppage ) {
    this->ppage = ppage;
}

int WStatPageHeader::getTotalLines() {
    return 2;
}

void WStatPageHeader::execute( int lnl, int lnr, RowList &data ) {
    data.clear();
    for ( int line = lnl; line < lnr; line++ ) {
        Row r;
        r.resize( 7 );
        WLabel* report_status = new WLabel();
        switch ( line ) {
        case 0:
            WLineEdit* pid;
            WLineEdit* phone;
            WDatePicker* date_from, *date_to;
            WLineEdit* text;
            WComboBox* status;
            WVBoxLayout *wDateGrp;
            WHBoxLayout *wDateLblGrp_from;
            WHBoxLayout *wDateLblGrp_to;
            WContainerWidget* date_from_group;
            WContainerWidget* date_to_group;
            WContainerWidget* date;
            WPushButton* reportbtn;

            //Pid input field
            pid = new WLineEdit(); pid->setMaximumSize(  WLength( 1, WLength::Centimeter ), WLength::Auto  );
            //Phone input field
            phone = new WLineEdit();
            //Date input field
            date_from = new WDatePicker(); date_from->setDate( WDate::currentDate().addDays( -7 ) );
            date_to = new WDatePicker(); date_to->setDate( WDate::currentDate() );
            wDateGrp = new WVBoxLayout();
            wDateLblGrp_from = new WHBoxLayout();
            wDateLblGrp_from->addWidget( new WLabel( WString::fromUTF8("С") ) );
            wDateLblGrp_from->addWidget( date_from );
            wDateLblGrp_to = new WHBoxLayout();
            wDateLblGrp_to->addWidget( new WLabel( WString::fromUTF8("По") ) );
            wDateLblGrp_to->addWidget( date_to );
            date_from_group = new WContainerWidget();
            date_from_group->setLayout( wDateLblGrp_from, AlignMiddle | AlignCenter );
            wDateGrp->addWidget( date_from_group );
            date_to_group = new WContainerWidget();
            date_to_group->setLayout( wDateLblGrp_to, AlignMiddle | AlignCenter );
            wDateGrp->addWidget( date_to_group );
            date = new WContainerWidget();
            date->setLayout( wDateGrp, AlignMiddle | AlignCenter );
            //Message text field
            text = new WLineEdit();
            text->setMinimumSize( WLength( 6, WLength::Centimeter ), WLength::Auto );
            //Delivery status field
            status = new WComboBox();
            status->addItem(WString::fromUTF8("Любой"));
            status->addItem(WString::fromUTF8("Доставлено"));
            status->addItem(WString::fromUTF8("Не доставлено"));
            //Report button
            reportbtn = new WPushButton( WString::fromUTF8("Сгенерировать отчет") );
            reportbtn->clicked().connect(boost::bind(
                                             &PersonalPage::onReportBtnClicked,
                                             ppage,
                                             pid,
                                             phone,
                                             date_from,
                                             date_to,
                                             text,
                                             status,
                                             reportbtn,
                                             report_status
                                             )
                                         );

            r.push_back( boost::shared_ptr< WWidget >( pid ) );
            r.push_back( boost::shared_ptr< WWidget >( phone ) );
            r.push_back( boost::shared_ptr< WWidget >( date ) );
            r.push_back( boost::shared_ptr< WWidget >( text ) );
            r.push_back( boost::shared_ptr< WWidget >( status ) );
            r.push_back( boost::shared_ptr< WWidget >( /*empty*/ ) );
            r.push_back( boost::shared_ptr< WWidget >( reportbtn ) );
            break;

        case 1:
            r.push_back( boost::shared_ptr< WWidget >( new WLabel(WString::fromUTF8("IDP")) ) );
            r.push_back( boost::shared_ptr< WWidget >( new WLabel(WString::fromUTF8("Телефон")) ) );
            r.push_back( boost::shared_ptr< WWidget >( new WLabel(WString::fromUTF8("Дата")) ) );
            r.push_back( boost::shared_ptr< WWidget >( new WLabel(WString::fromUTF8("Текст")) ) );
            r.push_back( boost::shared_ptr< WWidget >( new WLabel(WString::fromUTF8("Статус")) ) );
            r.push_back( boost::shared_ptr< WWidget >( new WLabel(WString::fromUTF8("Цена")) ) );
            r.push_back( boost::shared_ptr< WWidget >( report_status ) );

            break;
        }
        data.push_back( r );
    }
}

WApplication *createPersonalPage(const WEnvironment& env) {
  /*
   * You could read information from the environment to decide whether
   * the user has permission to start a new application
   */
  return new PersonalPage(env);
}

PersonalPage::PersonalPage( const WEnvironment& env ):WApplication( env ) {

    this->useStyleSheet( "/resources/css/PersonalPage.css" );
    authorized = false;

    WContainerWidget* wLoginBox = buildLoginPage( env );
    root()->addWidget( wLoginBox );
}

WContainerWidget* PersonalPage::buildLoginPage( const WEnvironment &env ) {
    setTitle( WString::fromUTF8("GreenSMS: Страница авторизации") );
    setCssTheme("polished");

    WString failedMsg = WString::fromUTF8( "Логин или пароль неверны" );
    WString greetMsg = WString::fromUTF8( "Представьтесь, пожалуйста" );
    WString unameMsg = WString::fromUTF8( "Ваш логин" );
    WString passMsg = WString::fromUTF8( "Ваш пароль" );
    WString loginMsg = WString::fromUTF8( "Вход" );

    WLabel* failedMsgLabel = new WLabel( failedMsg );
    WLabel* greetMsgLabel = new WLabel( greetMsg );
    WHintLineEdit* loginBox = new WHintLineEdit( unameMsg );
    WHintLinePassEdit* passBox = new WHintLinePassEdit( passMsg );
    WPushButton* loginBtn = new WPushButton( loginMsg );

    failedMsgLabel->setObjectName( "wLoginBlockFailedMsgLabel" );
    failedMsgLabel->setStyleClass( "wLoginBlockFailedMsgLabel" );
    failedMsgLabel->setHidden( true );

    loginBox->setObjectName( "wLoginBlockLoginBox" );
    loginBox->enterPressed().connect(SLOT(passBox, WHintLinePassEdit::setFocus));

    passBox->setObjectName( "wLoginBlockPassBox" );
    passBox->enterPressed().connect(SLOT(this, PersonalPage::onLogin));

    loginBtn->setMaximumSize( WLength( 50, WLength::Percentage ), WLength::Auto );
    loginBtn->clicked().connect(SLOT(this, PersonalPage::onLogin));

    WVBoxLayout *wLoginGrp = new WVBoxLayout();
    wLoginGrp->addWidget( greetMsgLabel );
    wLoginGrp->addWidget( loginBox );
    wLoginGrp->addWidget( passBox );
    wLoginGrp->addWidget( failedMsgLabel );
    wLoginGrp->addWidget( loginBtn );
    WContainerWidget* wLoginBlock = new WContainerWidget();
    wLoginBlock->setObjectName( "wLoginBlock" );
    wLoginBlock->setLayout( wLoginGrp, AlignMiddle | AlignCenter );

    return wLoginBlock;
}

void PersonalPage::onLogin() {
    authorized = false;

    WContainerWidget* wLoginBlock = static_cast<WContainerWidget*>( root()->find( "wLoginBlock" ) );
    if ( !wLoginBlock )
        return;

    WHintLineEdit* loginBox = static_cast<WHintLineEdit*>( root()->find( "wLoginBlockLoginBox" ) );
    if ( !loginBox )
        return;

    WHintLinePassEdit* passBox = static_cast<WHintLinePassEdit*>( root()->find( "wLoginBlockPassBox" ) );
    if ( !passBox )
        return;

    WLabel* failedMsgLabel = static_cast<WLabel*>( root()->find( "wLoginBlockFailedMsgLabel" ) );
    if ( !failedMsgLabel )
        return;

    try {
        PartnerInfo p = PartnerManager::get_mutable_instance().findByName( loginBox->text().toUTF8() );
        authorized = ( p.pPass == passBox->text().toUTF8() );
        pId = p.pId;
    } catch ( ... ) {
        return;
    }

    if ( !authorized ) {
        failedMsgLabel->setHidden( false );
        passBox->setText( "" );
        passBox->setFocus( );
    } else {
        root()->removeWidget( wLoginBlock );
        buildPersonalPage();
    }
}

void PersonalPage::onReportBtnClicked(
        WLineEdit* pid,
        WLineEdit* phone,
        WDatePicker* date_from,
        WDatePicker* date_to,
        WLineEdit* text,
        WComboBox* status,
        WPushButton* reportbtn,
        WLabel* report_status ) {

}


void PersonalPage::buildPersonalPage( ) {
    setTitle( WString::fromUTF8("GreenSMS: Личный кабинет ") );

    this->useStyleSheet( "/resources/css/resp_table.css" );

    tbl = new WTable();
    tbl->setHeaderCount(2);
    reportbtn = new WPushButton( WString::fromUTF8("Сгенерировать отчет") );
    reportbtn->clicked().connect(SLOT(this, PersonalPage::onReportGenerate));

    reportstatus = new WLabel();

    pid = new WLineEdit(); pid->setMaximumSize(  WLength( 1, WLength::Centimeter ), WLength::Auto  );
    phone = new WLineEdit();
    date_from = new WDatePicker(); date_from->setDate( WDate::currentDate().addDays( -7 ) );
    date_to = new WDatePicker(); date_to->setDate( WDate::currentDate() );
    text = new WLineEdit();
    text->setMinimumSize( WLength( 6, WLength::Centimeter ), WLength::Auto );
    status = new WComboBox();
    status->addItem(WString::fromUTF8("Любой"));
    status->addItem(WString::fromUTF8("Доставлено"));
    status->addItem(WString::fromUTF8("Не доставлено"));
    tbl->elementAt( 0, 1 )->addWidget( phone );
    tbl->elementAt( 0, 2 )->addWidget( new WLabel( WString::fromUTF8("С") ) );
    tbl->elementAt( 0, 2 )->addWidget( date_from );
    tbl->elementAt( 0, 2 )->addWidget( new WLabel( WString::fromUTF8("По") ) );
    tbl->elementAt( 0, 2 )->addWidget( date_from );
    tbl->elementAt( 0, 2 )->addWidget( date_to );
    tbl->elementAt( 0, 3 )->addWidget( text );
    tbl->elementAt( 0, 4 )->addWidget( status );
    tbl->elementAt( 0, 6 )->addWidget( reportbtn );

    tbl->elementAt( 1, 0 )->addWidget( new WLabel(WString::fromUTF8("IDP")) );
    tbl->elementAt( 1, 1 )->addWidget( new WLabel(WString::fromUTF8("Телефон")) );
    tbl->elementAt( 1, 2 )->addWidget( new WLabel(WString::fromUTF8("Дата")) );
    tbl->elementAt( 1, 3 )->addWidget( new WLabel(WString::fromUTF8("Текст")) );
    tbl->elementAt( 1, 4 )->addWidget( new WLabel(WString::fromUTF8("Статус")) );
    tbl->elementAt( 1, 5 )->addWidget( new WLabel(WString::fromUTF8("Цена")) );
    tbl->elementAt( 1, 6 )->addWidget( reportstatus );

    root()->addWidget( tbl );

}

PersonalPage::MsgidList PersonalPage::genMsgIds( const std::string& _idp, const std::string& phone, const std::string& _ldate, const std::string& _rdate, const std::string& _text, int page = 0 ) {
    PGSql::ConnectionHolder cHold( db );
    ConnectionPTR conn = cHold.get();
    TransactionPTR tr = db.openTransaction( conn, "PersonalPage::genMsgIds" );

    std::ostringstream req;
    req << "SELECT \"REQUESTID\", \"TO\", \"WHEN\" FROM smsrequest WHERE TRUE ";
    if ( !phone.empty() ) {
        req << "AND \"TO\" LIKE '%" << phone << "%' ";
    }
    if ( !( _ldate == "Null" ) ) {
        boost::gregorian::date date = boost::gregorian::from_string( _ldate );
        boost::gregorian::date orig( 1970, boost::gregorian::Jan, 1 );
        boost::posix_time::ptime from( date, boost::posix_time::hours(0) );
        boost::posix_time::ptime begin( orig, boost::posix_time::hours(0) );

        boost::posix_time::time_period lv( begin, from );
        req << "AND \"WHEN\" > '" << lv.length().total_seconds()-3*60*60 << "' ";
    }

    if ( !( _rdate == "Null" ) ) {
        boost::gregorian::date date = boost::gregorian::from_string( _rdate );
        boost::gregorian::date orig( 1970, boost::gregorian::Jan, 1 );
        boost::posix_time::ptime from( date, boost::posix_time::hours(0) );
        boost::posix_time::ptime to = from + boost::posix_time::hours(24);
        boost::posix_time::ptime begin( orig, boost::posix_time::hours(0) );

        boost::posix_time::time_period rv( begin, to );
        req << "AND \"WHEN\" < '" << rv.length().total_seconds()-3*60*60 << "' ";
    }


    if ( !_text.empty() ) {
        req << "AND \"TXT\" LIKE '%" << utils::escapeString( tr->esc( _text ), "%_", "\\" ) << "%' ";
        req << "ESCAPE '\\\\' ";

    }

    if ( !_idp.empty() ) {
        req << "AND \"PID\"='" << tr->esc( _idp ) << "' ";
    }

    req << "ORDER BY \"WHEN\" DESC limit 20000; ";

    std::string a = req.str();


    MsgidList l;

    Result res = tr->exec( req.str() );
    tr->commit();
    for ( Result::const_iterator it = res.begin(); it != res.end(); it++ ) {
        std::string to;
        sms::to_vec tov;
        bool found;

        std::cout << (*it)[1].to( to );
        sms::utils::Tokenize(to, tov, ",");

        for (unsigned int i = 0; i < tov.size(); i++) {
            found = tov[i].find(phone);
            if (found != std::string::npos) {
                l.push_back(std::make_pair(sms::SMSMessage::ID((*it)[0].as<long long>(), i), tov[i]));
            }
        }
    }

    return l;
}

PersonalPage::ReqResp PersonalPage::genReq( const MsgidList& list, int status ) {

    MsgidList::const_iterator it;

    ReqResp r;

    for ( it = list.begin(); it != list.end(); it++ ) {
        PGSql::ConnectionHolder cHold( db );
        ConnectionPTR conn = cHold.get();
        TransactionPTR tr = db.openTransaction( conn, "PersonalPage::genReq" );
        std::ostringstream req;
        req     << "SELECT \"smsrequest\".\"PID\", \"message_status\".\"WHEN\", \"smsrequest\".\"TXT\", \"message_status\".\"STATUS\" "
                << "FROM \"smsrequest\", \"message_status\" WHERE \"smsrequest\".\"REQUESTID\" = \"message_status\".\"REQUESTID\" "
                << "AND \"message_status\".\"REQUESTID\" = '" << it->first.req << "' "
                << "AND \"message_status\".\"MESSAGEID\" = '" << it->first.msg_num << "' ;";

        Result res = tr->exec( req.str() );
        tr->commit();
        for ( Result::const_iterator dbr = res.begin(); dbr != res.end(); dbr++ ) {
            std::string s;
            std::vector< std::string > row;

            row.push_back( it->first.to_str() );
            row.push_back((*dbr)[0].as<string>());
            row.push_back(it->second);
            boost::gregorian::date orig( 1970, boost::gregorian::Jan, 1 );
            boost::posix_time::ptime dt( orig, boost::posix_time::seconds( (*dbr)[1].as<long>()+3*60*60 ) );

            row.push_back( boost::posix_time::to_simple_string(dt) );
            row.push_back( (*dbr)[2].as<std::string>() );
            int statusid;

            switch ( (*dbr)[3].as<int>() ) {
            case -3:
                if ( status == 1 )
                    continue;
                row.push_back( "Неверный номер" );
                break;
            case -2:
                if ( status == 1 )
                    continue;
                row.push_back( "Таймаут" );
                break;
            case -1:
                if ( status == 1 )
                    continue;
                row.push_back( "Не доставлено" );
                break;
            case 0:
                if ( status == 2 )
                    continue;
                row.push_back( "Доставлено" );
                break;
            default:
                if ( status != 0 )
                    continue;
                row.push_back( "В процессе доставки" );
            }
            try {
                PartnerInfo p = PartnerManager::get_mutable_instance().findById( (*dbr)[0].as<string>() );
                SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( it->first );
                float price = p.tariff.costs( msg->getMsgClass().country, msg->getMsgClass().opcode );
                row.push_back( boost::lexical_cast< string >( int(floor( price * 100 )) / 100) + string(".") +  boost::lexical_cast< string >( int(floor( price * 100 )) % 100 ) );
            } catch ( std::exception& err ) {
                row.push_back( "1.21" );
                Logger::get_mutable_instance().smslogwarn( err.what() );
            }

            r.push_back(row);
        }
    }

    return r;
}

void PersonalPage::onReportGenerate() {

    reportbtn->disable();
    reportstatus->setText(WString::fromUTF8("Идет генерация данных"));

    while ( tbl->rowCount() > 2) {
        //tbl->rowAt( 2 )->
        tbl->deleteRow( 2 );
    }
    this->processEvents();

    try {
        std::string _phone = phone->text().toUTF8();
        std::string _ldate = date_from->date().toString("yyyy/MM/dd").toUTF8();
        std::string _rdate = date_to->date().toString("yyyy/MM/dd").toUTF8();
        std::string _text = text->text().toUTF8();
        std::string _pid = pId;

        int _status = status->currentIndex();

        MsgidList list = genMsgIds( _pid, _phone, _ldate,_rdate, _text );

        ReqResp req = genReq( list, _status );

        for ( unsigned int i = 0; i < req.size(); i++ ) {


            if ( req[i][5] == "Доставлено" ) {
                tbl->rowAt( i+2 )->setStyleClass( "rowOk" );
            } else
            if ( req[i][5] == "В процессе доставки" ) {
                tbl->rowAt( i+2 )->setStyleClass( "rowWarn" );
            } else {
                tbl->rowAt( i+2 )->setStyleClass( "rowErr" );
            }

            tbl->elementAt( i+2, 0 )->addWidget( new WText( WString::fromUTF8(req[i][1] )) );
            tbl->elementAt( i+2, 1 )->addWidget( new WText( WString::fromUTF8(req[i][2] )) );
            tbl->elementAt( i+2, 2 )->addWidget( new WText( WString::fromUTF8(req[i][3] )) );

            WString txt = WString::fromUTF8( req[i][4] );

            WText* tr = new WText( WString::fromUTF8(req[i][4]) );
            tbl->elementAt( i+2, 3 )->addWidget( tr );

            //tbl->elementAt( i+2, 3 )->addWidget( new WText( req[i][4] ) );
            tr = new WInfoText( WString::fromUTF8(req[i][5]) );
            tbl->elementAt( i+2, 4 )->addWidget( tr );
            //tbl->elementAt( i+2, 4 )->setColumnSpan( 2 );

            if ( req[i][5] == "Неверный номер" ) {
                tbl->elementAt( i+2, 5 )->addWidget( new WText( "0.00" ) );
            } else {
                tbl->elementAt( i+2, 5 )->addWidget( new WText( WString::fromUTF8(req[i][6] )) );
            }

        }
    } catch ( PGBrokenConnection& err ) {
        reportbtn->enable();
        reportstatus->setText(WString::fromUTF8(string("Ошибка соеднинения с базой") + err.what()));
        return;
    } catch ( PGSqlError& err ) {
        reportbtn->enable();
        reportstatus->setText(WString::fromUTF8(string("Ошибка SQL запроса") + err.what()));
        return;
    }

    reportbtn->enable();
    reportstatus->setText(WString::fromUTF8("Генерации отчета завершена"));
}
