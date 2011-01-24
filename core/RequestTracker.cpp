/*
 * RequestTracker.cpp
 *
 *  Created on: 22.01.2010
 *      Author: mohtep
 */

#include <cstdio>
#include <iostream>
#include "RequestTracker.h"
#include "ConfigManager.h"
#include "SMPPGateManager.h"
#include "StatManager.h"
#include "PartnerManager.h"
#include "utils.h"

#include "Queue.h"

RequestTracker* RequestTracker::pInstance_ = 0;

void RequestTracker::deliverUndelivered() {
    std::string idp = "";
    unsigned int ma_p = 0;

    boost::xtime now;
    boost::xtime_get( &now, boost::TIME_UTC );
    try {
        std::ostringstream r;

        r       << "SELECT \"REQUESTID\", \"MESSAGEID\", \"WHEN\" FROM message_status "
                << "WHERE \"WHEN\">'" << now.sec - ConfigManager::Instance()->getProperty<int>( "system.undeliveredtimeout" ) << "' "
                << "AND NOT \"STATUS\"='-1' "
                << "AND NOT \"STATUS\"='0';";


        PGSql::ConnectionHolder cHold( db );
        ConnectionPTR conn = cHold.get();
        TransactionPTR tr = db.openTransaction( conn, "RequestTracker::RequestTracker()" );
        Result res = tr->exec( r.str() );
        tr->commit();
        for ( Result::const_iterator it = res.begin(); it != res.end(); it++ ) {
            SMSMessage::ID msgid;
            msgid.req = (*it)[0].as<long long>();
            msgid.msg_num = (*it)[1].as<int>();
            SMSRequest::PTR req;
            idp = "";
            try {
                req = this->loadRequestFromDb(msgid.req);
                req_cache.push( req, msgid.req );
                SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );
                std::string idp = req->pid;
            } catch (...) {
                continue;
            }

            del_queue.push( SMSOperation::create<OP_CheckDelivery>( std::make_pair( req, msgid ), idp, ma_p, OP_CheckDeliveryP ), ConfigManager::Instance()->getProperty<int>( "system.resendtimeout" ) );
            //del_queue.push( SMSOperation::create<OP_MarkUndelivered>( std::make_pair( req, msgid ), idp, ma_p, OP_MarkUndeliveredP ), ConfigManager::Instance()->getProperty<int>( "system.undeliveredtimeout" ) + (*it)[2].as<int>() - now.sec );

        }
    } catch ( PGBrokenConnection& err ) {
        Logger::get_mutable_instance().smslogerr( string("RequestTracker::RequestTracker() ") + err.what() );
        return;
    } catch ( PGSqlError& err ) {
        Logger::get_mutable_instance().smslogerr( string("RequestTracker::RequestTracker() ") + err.what() );
        return;
    }

}

void RequestTracker::markUndelivered() {
    boost::xtime now;
    boost::xtime_get( &now, boost::TIME_UTC );

    try {
        std::ostringstream r;

        r       << "UPDATE message_status "
                << "SET \"STATUS\"='-1' "
                << "WHERE \"WHEN\"<='" << now.sec - 24*60*60 << "' "
                << "AND NOT \"STATUS\"='-1' "
                << "AND NOT \"STATUS\"='0';";

        PGSql::ConnectionHolder cHold( db );
        ConnectionPTR conn = cHold.get();
        TransactionPTR tr = db.openTransaction( conn, "RequestTracker::RequestTracker()" );
        tr->exec( r.str() );
        tr->commit();
    } catch ( PGBrokenConnection& err ) {
        Logger::get_mutable_instance().smslogerr( string("RequestTracker::RequestTracker() ") + err.what() );
        return;
    } catch ( PGSqlError& err ) {
        Logger::get_mutable_instance().smslogerr( string("RequestTracker::RequestTracker() ") + err.what() );
        return;
    }

}

RequestTracker::RequestTracker():
        kannel( HttpClient() ),
        db( PGSqlConnPoolSystem::get_mutable_instance().getdb() ),
        op_queue( TOpQueue() ),
        req_cache( TReqCache(ConfigManager::Instance()->getProperty<long>( "system.cache.size" )) ),
        del_queue( TDelQueue() ),
        kserver( ConfigManager::Instance()->getProperty<std::string>("kannel.server") ),
        kport( ConfigManager::Instance()->getProperty<std::string>("kannel.port") ),
        out_queue( TOutBoxQueuePTR( new TOutBoxQueue() ) ) {

    ConfigManager* cm = ConfigManager::Instance();

    // Проверить статус для всех сообщений с промежуточным статусом
    Timer::Instance()->addSingleEvent( boost::bind( &RequestTracker::deliverUndelivered, this ), 0 );

    // Установить статус "недоставлен" для всех сообщений с промежуточным статусом старее суток
    Timer::Instance()->addSingleEvent( boost::bind( &RequestTracker::markUndelivered, this ), 0 );

}

RequestTracker::~RequestTracker() {
}

SMSRequest::ID RequestTracker::registerRequest( SMSRequest::PTR req ) {
    std::string idp = req->pid;
    unsigned int ma_p = 0;
    unsigned char mi_p = OP_NewRequestP;
    op_queue.push( SMSOperation::create<OP_NewRequest>( req, idp, ma_p, mi_p ), ma_p, mi_p );
    return req->getID();
}

void RequestTracker::registerDeliveryNotification( SMSMessage::ID id, SMSMessage::Status code, std::string gate ) {
    unsigned int ma_p = 99;
    unsigned char mi_p = OP_NewDeliveryP;
    op_queue.push( SMSOperation::create<OP_NewDelivery>( std::make_pair( id, DeliveryInfo( code, gate ) ) ), ma_p, mi_p );
}

SMSRequest::PTR RequestTracker::loadRequestFromDb( SMSRequest::ID reqid ) {
    SMSRequest* preq;
    SMSRequest::PTR req;
    {
        std::ostringstream r;

        r       << "SELECT "
        <<"\"USER\", "
        <<"\"PASS\", "
        <<"\"TO\", "
        <<"\"TXT\", "
        <<"\"TID\", "
        <<"\"FROM\", "
        <<"\"UTF\", "
        <<"\"SUBPREF\", "
        <<"\"HEX\", "
        <<"\"UDH\", "
        <<"\"DELAY\", "
        <<"\"DLR\", "
        <<"\"PID\", "
        <<"\"PRIORITY\", "
        <<"\"GARANT\", "
        <<"\"WHEN\" "
        << " from smsrequest WHERE \"REQUESTID\"='" << reqid << "';";

        PGSql::ConnectionHolder cHold( db );
        ConnectionPTR conn = cHold.get();
        TransactionPTR tr = db.openTransaction( conn, "RequestTracker::loadRequestFromDb" );
        Result res = tr->exec( r.str() );
        tr->commit();
        if ( res.size() > 0 ) {
            to_vec tov;
            utils::Tokenize( res[0][2].as<std::string>(), tov, ",");
            preq = new SMSRequest(
                res[0][0].as<std::string>(),
                res[0][1].as<std::string>(),
                tov,
                res[0][3].as<std::string>(),
                res[0][4].as<std::string>(),
                res[0][5].as<std::string>(),
                res[0][6].as<std::string>(),
                res[0][7].as<std::string>(),
                res[0][8].as<std::string>(),
                res[0][9].as<std::string>(),
                res[0][10].as<std::string>(),
                res[0][11].as<std::string>(),
                res[0][12].as<std::string>(),
                res[0][13].as<int>(),
                res[0][14].as<std::string>()
            );
            req = SMSRequest::PTR( preq );

            req_cache.push( req, reqid );

        } else {
            throw PGSqlError( "Empty dataset" );
        }
    }

    return req;
}

void RequestTracker::parseNewRequestEvent( SMSRequest::PTR req ) {
    req_cache.push( req, req->getID() );
    std::string idp = req->pid;
    int rdelay = ConfigManager::Instance()->getProperty<int>( "system.resendtimeout" ); // TODO
    unsigned int ma_p = 0;

    std::ostringstream out;
    out << "OP_NewRequest ID=" << req->getID() << " parts=" << req->parts << " ";
    try {
        PGSql::ConnectionHolder cHold( db );
        ConnectionPTR conn = cHold.get();
        TransactionPTR tr = db.openTransaction( conn, "RequestTracker::parseNewRequestEvent" );

        boost::xtime now;
        boost::xtime_get( &now, boost::TIME_UTC );
        std::ostringstream r;

        r       << "INSERT INTO smsrequest "
        << "(\"REQUESTID\",\"USER\",\"PASS\",\"TO\",\"TXT\",\"TID\",\"FROM\",\"UTF\",\"SUBPREF\",\"HEX\",\"UDH\",\"DELAY\",\"DLR\",\"PID\",\"PRIORITY\",\"GARANT\",\"WHEN\") "
        << "VALUES('"
        << req->getID()<< "','"
        << req->uname<< "','"
        << req->pass<< "','"
        << sms::utils::str_join( req->to, ",") << "','"
        << tr->esc( req->msg ) << "','"
        << req->tid<< "','"
        << tr->esc( req->from )<< "','"
        << req->utf<< "','"
        << req->subpref<< "','"
        << req->hex<< "','"
        << req->udh<< "','"
        << req->delay<< "','"
        << req->dlr<< "','"
        << req->pid<< "','"
        << req->priority<< "','"
        << req->garant<< "','"
        << now.sec << "');";
        tr->exec( r.str() );
        tr->commit();
    } catch ( PGBrokenConnection& err ) {
        out << "Retrying;\n" << err.what();
        del_queue.push( SMSOperation::create<OP_NewRequest>( req, idp, ma_p, OP_NewRequestP ), rdelay );
        Logger::get_mutable_instance().smslogwarn( out.str() );
        return;
    } catch ( PGSqlError& err ) {
        out << "Denied;\n" << err.what();
        Logger::get_mutable_instance().smslogerr( out.str() );
        return;
    }

    for ( unsigned int i = 0; i < req->to.size(); i++ ) {
        SMSMessage::ID  msgid = SMSMessage::ID( req->getID(), i );
        SMSMessageManager::get_mutable_instance().createMessage( msgid, req->to[i], req->parts );

        op_queue.push( SMSOperation::create <OP_NewMessage>( std::make_pair( req, msgid ), idp, ma_p, OP_NewMessageP ), ma_p, OP_NewMessageP );
    }

    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );
}

void RequestTracker::parseNewMessageEvent( SMSRequest::PTR req, SMSMessage::ID msgid ) {
    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );
    std::string idp = req->pid;
    unsigned int ma_p = 0;

    std::ostringstream out;
    out     << "OP_NewMessage ID=" << msg->getID().to_str() << " "
            << "country=" << msg->getMsgClass().country << " "
            << "status=" << msg->getStatus()() << " ";

    op_queue.push( SMSOperation::create<OP_SendMessage>( std::make_pair( req, msgid ), idp, ma_p, OP_SendMessageP  ), ma_p, OP_SendMessageP );

    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );
}

void RequestTracker::parseCheckDeliveryEvent( SMSRequest::PTR req, SMSMessage::ID msgid ) {

    std::string idp = req->pid;
    unsigned int ma_p = 0;
    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );
    std::ostringstream out;
    out << "OP_CheckDelivery ID="
    << msg->getID().to_str() << " "
    << "status=" << msg->getStatus()() << " ";

    if ( msg->getStatus() < SMSMessage::Status::ST_BUFFERED ) {
        out << "Final status has not been reached; resending; ";
        op_queue.push( SMSOperation::create<OP_SendMessage>( std::make_pair( req, msgid ), idp, ma_p, OP_SendMessageP ), ma_p, OP_SendMessageP );
    }

    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );

}

void RequestTracker::parseCheckAckEvent( SMSRequest::PTR req, SMSMessage::ID msgid, string gname ) {

    std::string idp = req->pid;
    unsigned int ma_p = 0;

    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );

    std::ostringstream out;
    bool ack_found = false;

    out << "OP_CheckACK ID="
            << msg->getID().to_str() << " "
            << "status=" << msg->getStatus()() << " "
            << "gate=" << gname << " ";

    SMSMessage::HistoryType hst = msg->getHistory();
    SMSMessage::HistoryType::iterator it;


    for ( it = hst.begin(); it != hst.end(); it++ ) {
        if ( ( it->op_code == 1 ) && ( it->op_direction == 1 ) && ( it->gateway == gname ) )
            ack_found = true;
    }

    if ( !ack_found ) {
        out << "ACK not found; resending; ";
        op_queue.push( SMSOperation::create<OP_SendMessage>( std::make_pair( req, msgid ), idp, ma_p, OP_SendMessageP ), ma_p, OP_SendMessageP );
    } else {
        out << "ACK found ";
    }

    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );

}

void RequestTracker::parseMarkUndeliveredEvent( SMSRequest::PTR req, SMSMessage::ID msgid ) {

    std::string idp = req->pid;
    unsigned int ma_p = 0;
    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );

    std::ostringstream out;
    out << "OP_MarkUndelivered ID="
    << msg->getID().to_str() << " "
    << "status=" << msg->getStatus()() << " ";

    if ( msg->getStatus() < SMSMessage::Status::ST_NOT_DELIVERED ) {
        std::string gates;
        boost::xtime now;
        boost::xtime_get( &now, boost::TIME_UTC );
        out << "Message expired; ";
        SMSMessage::HistoryType::const_iterator it;
        SMSMessage::HistoryType hst = msg->getHistory();
        for ( it = hst.begin(); it != hst.end(); it++ ) {
            if ( it->op_code == 0 )  {
                gates += it->gateway+";";
            }
        }
        op_queue.push(SMSOperation::create<OP_NewHistoryElement > (std::make_pair(
                          msg->getID(),
                          SMSMessage::HistoryElement(1, 1, SMSMessage::Status::ST_EXPIRED, gates, now.sec)),
                          idp, ma_p, OP_NewHistoryElementP ),
                          ma_p, OP_NewHistoryElementP );
    }

    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );

}


void RequestTracker::parseDeliveryNotify( SMSRequest::PTR req, SMSMessage::ID msgid ) {
    std::string idp = req->pid;
    int rdelay = ConfigManager::Instance()->getProperty<int>( "system.resendtimeout" ); // TODO
    unsigned int ma_p = 0;

    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );

    std::ostringstream out, url, url2;
    out << "OP_DeliveryNotify ID=" << msgid.to_str() << " ";

    try {
        {
            SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );
            url << "http://ftp.smsonline.ru/sys/jane-dlr.cgi?" <<
                    "phone=" << req->to[ msg->getID().msg_num ] <<
                    "&tid=" << req->tid  <<
                    "&from=" << utils::UrlEncodeString(req->from)  <<
                    "&status=" << msg->getStatus()();
            url2<< "http://smsdelivery.odnoklassniki.ru:8040/dlr/?" <<
                    "phone=" << req->to[ msg->getID().msg_num ] <<
                    "&sms_id=" << req->tid  <<
                    "&from=" << utils::UrlEncodeString(req->from)  <<
                    "&dlr_status=" << msg->getStatus()();
        }

        HttpClient::Response resp = kannel.get( url.str() );
        if ( ( req->pid == "745" ) && ( req->from == "Odkl.ru" ) ) {
            HttpClient::Response resp = kannel.get( url2.str() );
        }

    } catch ( HttpError err ) {
        out << "Retrying;\n" << boost::diagnostic_information( err );
        del_queue.push( SMSOperation::create<OP_DeliveryNotify>( std::make_pair( req, msgid ), idp, ma_p, OP_DeliveryNotifyP ), rdelay );
        Logger::get_mutable_instance().smslogwarn( out.str() );
        return;
    }

    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );
}

void RequestTracker::parseMessage2kannelEvent( SMSRequest::PTR req, SMSMessage::ID msgid ) {
    std::string idp = req->pid;
    int rdelay = ConfigManager::Instance()->getProperty<int>( "system.resendtimeout" );; // TODO
    unsigned int ma_p = 0;
    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );

    std::ostringstream out;
    out << "OP_SendMessage ID=" << msg->getID().to_str() << " phase 2: queued to delivery ";

    boost::xtime now;
    boost::xtime_get( &now, boost::TIME_UTC );
    SMPPGateManager* gateManager = SMPPGateManager::Instance();

    try {
        gateManager->pushToQueue( req, msgid );
    } catch ( NoMoreGates err ) {
        out << "Denied; Message cannot be delivered";
        std::string gates;

        SMSMessage::HistoryType::const_iterator it;
        SMSMessage::HistoryType hst = msg->getHistory();
        for ( it = hst.begin(); it != hst.end(); it++ ) {
            if ( it->op_code == 0 )  {
                gates += it->gateway+";";
            }
        }
        op_queue.push(SMSOperation::create<OP_NewHistoryElement > (std::make_pair(
                          msg->getID(),
                          SMSMessage::HistoryElement(2, 1, SMSMessage::Status::ST_REJECTED, gates, now.sec)),
                          idp, ma_p, OP_NewHistoryElementP ),
                          ma_p, OP_NewHistoryElementP );
        Logger::get_mutable_instance().smslogerr( out.str() );
        return;
    }
    
    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );
}

void RequestTracker::parseNewHistoryElement( SMSMessage::ID msg_id, SMSMessage::HistoryElement element ) {
    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msg_id );
    SMSRequest::PTR req;

    int rdelay = ConfigManager::Instance()->getProperty<int>( "system.resendtimeout" );; // TODO
    std::string idp = ""; //TODO
    unsigned int ma_p = 0;

    std::ostringstream out;
    out     << "OP_NewHistoryElement ID=" << msg_id.to_str() << " "
            << "gate=" << element.gateway << " "
            << "op_code=" << element.op_code << " "
            << "op_result=" << element.op_result() << " ";

    try {

        if (req_cache.exists(msg_id.req)) {
            req = req_cache.get(msg_id.req);
        } else {
            req = loadRequestFromDb( msg_id.req );
        }

        idp = req->pid; //TODO
        ma_p = 0;

        msg->addHistoryElement( element );

        if ( msg->getStatus() < element.op_result ) {
            if ( msg->getStatus() == SMSMessage::Status::ST_REJECTED ) {
                if ( element.op_code == 2 ) {
                    msg->setStatus( element.op_result );
                    out << "updated";
                } else {
                    out << "ignored";
                }
            } else {
                msg->setStatus( element.op_result );
                out << "updated";
            }
        } else {
            out << "ignored";
        }

    } catch ( PGBrokenConnection& err ) {
        out << "Retrying;\n" << err.what();
        del_queue.push( SMSOperation::create<OP_NewHistoryElement>( std::make_pair( msg_id, element ), idp, ma_p, OP_NewHistoryElementP ), rdelay );
        Logger::get_mutable_instance().smslogwarn( out.str() );
        return;
    } catch ( PGSqlError& err ) {
        out << "Denied;\n" << err.what();
        Logger::get_mutable_instance().smslogerr( out.str() );
        return;
    }


    Logger::get_mutable_instance().smsloginfo( out.str() );
}

void RequestTracker::parseNewDelivery( SMSMessage::ID msg_id, DeliveryInfo info ) {
    SMSRequest::PTR req;

    int rdelay = ConfigManager::Instance()->getProperty<int>( "system.resendtimeout" ); // TODO
    std::string idp = ""; //TODO
    unsigned int ma_p = 0;

    std::ostringstream out;
    out << "OP_NewDelivery ID=" << msg_id.to_str() << " ";
    try {
        SMSMessageManager::get_mutable_instance().loadMessage( msg_id );

        if (req_cache.exists(msg_id.req)) {
            req = req_cache.get( msg_id.req );
        } else {
            req = loadRequestFromDb( msg_id.req );
            req_cache.push( req, req->getID() );
        }

        idp = req->pid; //TODO
        ma_p = 0;

    } catch ( PGBrokenConnection& err ) {
        out << "Retrying;\n" << err.what();
        del_queue.push( SMSOperation::create<OP_NewDelivery>( std::make_pair( msg_id, info ), idp, ma_p, OP_NewDeliveryP ), rdelay );
        Logger::get_mutable_instance().smslogwarn( out.str() );
        return;
    } catch ( PGSqlError& err ) {
        out << "Denied;\n" << err.what();
        Logger::get_mutable_instance().smslogerr( out.str() );
        return;
    } catch ( MessageNotFoundError& err ) {
        out << "Denied;\n" << err.what();
        Logger::get_mutable_instance().smslogerr( out.str() );
        return;
    }

    boost::xtime now;
    boost::xtime_get(&now, boost::TIME_UTC);
    op_queue.push(SMSOperation::create<OP_NewHistoryElement > (std::make_pair(
                      msg_id,
                      SMSMessage::HistoryElement( 1, 1, info.status, info.gate, now.sec ) ),
                      idp, ma_p, OP_NewHistoryElementP), ma_p, OP_NewHistoryElementP );
    if ( info.status() == SMSMessage::Status::ST_REJECTED ) {
        op_queue.push( SMSOperation::create<OP_SendMessage > (std::make_pair( req, msg_id ), idp, ma_p, OP_SendMessage ), ma_p, OP_SendMessageP );
    }
    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );
}

void RequestTracker::MainEventLoop( ) {

    SMSOperation op;

    while (true) {
        try {
            while ( !op_queue.isEmpty() ) {
                {
                    TOpQueue::Lock lck( op_queue );
                    if ( !op_queue.isEmpty() ) {
                        op = op_queue.top();
                        op_queue.pop();
                    } else
                        continue;
                }

                if ( op.type() == OP_NewRequest ) {
                    parseNewRequestEvent( op.get<OP_NewRequest>() );
                }

                if ( op.type() == OP_NewMessage ) {
                    parseNewMessageEvent( op.get<OP_NewMessage>().first, op.get<OP_NewMessage>().second );
                }

                if ( op.type() ==  OP_SendMessage ) {
                    pushToSendingQueue( op.get<OP_SendMessage>().first, op.get<OP_SendMessage>().second );
                }

                if ( op.type() ==  OP_SubmitMessage ) {
                    SMPPGateManager::Instance()->send(
                            op.get<OP_SubmitMessage>().first,
                            op.get<OP_SubmitMessage>().second.first,
                            op.get<OP_SubmitMessage>().second.second );
                }

                if ( op.type() == OP_NewDelivery ) {
                    parseNewDelivery( op.get<OP_NewDelivery>().first, op.get<OP_NewDelivery>().second );
                }

                if ( op.type() == OP_CheckDelivery ) {
                    parseCheckDeliveryEvent( op.get<OP_CheckDelivery>().first, op.get<OP_CheckDelivery>().second );
                }

                if ( op.type() == OP_CheckACK ) {
                    parseCheckAckEvent( op.get<OP_CheckACK>().first.first, op.get<OP_CheckACK>().first.second, op.get<OP_CheckACK>().second );
                }

                if ( op.type() == OP_NewHistoryElement ) {
                    parseNewHistoryElement( op.get<OP_NewHistoryElement>().first, op.get<OP_NewHistoryElement>().second );
                }

                if ( op.type() == OP_DeliveryNotify ) {
                    //Timer::Instance()->addSingleEvent( boost::bind( &RequestTracker::parseDeliveryNotify, this, op.get<OP_DeliveryNotify>().first, op.get<OP_DeliveryNotify>().second ), 0 );
                    parseDeliveryNotify( op.get<OP_DeliveryNotify>().first, op.get<OP_DeliveryNotify>().second );
                }



            }

            boost::xtime xt;
            boost::xtime_get(&xt, boost::TIME_UTC);
            xt.nsec += 1e6;
            boost::thread::sleep(xt);
        } catch ( FailureError& err ) {
            Logger::get_mutable_instance().smslogerr( "Fatal error happened: " + boost::diagnostic_information( err ) );
        } catch ( RetryError& err ) {
            Logger::get_mutable_instance().smslogerr( "Temporary error happened: " + boost::diagnostic_information( err ) );
        } catch ( ... ) {
            Logger::get_mutable_instance().smslogerr( string("Unknown error happened: ") );
        }
    }
}

void RequestTracker::pushToSendingQueue( SMSRequest::PTR req, SMSMessage::ID msgid, unsigned int ma_p, unsigned char mi_p ) {

    SMSMessage::PTR msg = SMSMessageManager::get_mutable_instance().loadMessage( msgid );
    std::ostringstream out;
    out << "OP_SendMessage ID=" << msg->getID().to_str() << " phase 0: queued idp=[" << req->pid << "] ";

    string idp = req->pid;
    if ( pl_map.find( idp ) == pl_map.end() ) {
        boost::recursive_mutex::scoped_lock lck(lock);
        try {
            PartnerInfo ptnr = PartnerManager::get_mutable_instance().findById( idp );
            pl_map[ idp ] = TPartnersLimitQueuePTR( new TPartnersLimitQueue( ptnr.pLimit*5, 5 ) );
        } catch (...) {
            pl_map[ idp ] = TPartnersLimitQueuePTR( new TPartnersLimitQueue( 100, 5 ) );
        }

    }

    pl_map[ idp ]->push( std::make_pair( req, msgid ), req->parts, ma_p, mi_p );
    out << "parsed";
    Logger::get_mutable_instance().smsloginfo( out.str() );
}

void RequestTracker::DelayedEventLoop() {

    while (true) {
        try {

            {
                while ( true ) {
                    SMSOperation op;
                    {
                        TDelQueue::Lock lck( del_queue );
                            if ( del_queue.isEmpty() )
                                break;

                        op = del_queue.top();
                        del_queue.pop();
                    }
                    op_queue.push( op, op.majorPriority(), op.minorPriority() );
                }
            }

            boost::xtime xt;
            boost::xtime_get(&xt, boost::TIME_UTC);
            xt.sec++;
            boost::thread::sleep(xt);
        } catch ( FailureError& err ) {
            Logger::get_mutable_instance().smslogerr( "Fatal error happened: " + boost::diagnostic_information( err ) );
        } catch ( RetryError& err ) {
            Logger::get_mutable_instance().smslogerr( "Temporary error happened: " + boost::diagnostic_information( err ) );
        } catch ( std::runtime_error& err ) {
            Logger::get_mutable_instance().smslogerr( string("Unknown error happened: ") + err.what() );
        }
    }
}

void RequestTracker::OutboxPartnerEventLoop( ) {

    while (true) {
        try {
            TPartnersLimitMap::iterator it;
            {
                for ( it = pl_map.begin(); it != pl_map.end(); it++ ) {
                    TOutBoxQueue::Lock lck_out( *out_queue );

                    while ( true ) {
                        SMSRequest::PTR req;
                        SMSMessage::ID msgid;
                        std::ostringstream out;
                        {
                            TPartnersLimitQueue::Lock lck_pl( *it->second );
                            if ( it->second->isEmpty() )
                                break;
                            req = it->second->top().first;
                            msgid = it->second->top().second;

                            out << "OP_SendMessage ID=" << msgid.to_str() << " phase 1: accepted for delivery idp=[" << req->pid << "] ";

                            it->second->pop();
                        }
                        string idp = req->pid;
                        int idp_priority = 0;
                        out_queue->push( std::make_pair( req, msgid ), idp_priority );
                        //                parseMessage2kannelEvent( req, msg );
                        out << "parsed";
                        Logger::get_mutable_instance().smsloginfo( out.str() );
                    }
                }
            }

            boost::xtime xt;
            boost::xtime_get(&xt, boost::TIME_UTC);
            xt.sec++;
            boost::thread::sleep(xt);
        } catch ( FailureError& err ) {
            Logger::get_mutable_instance().smslogerr( "Fatal error happened: " + boost::diagnostic_information( err ) );
        } catch ( RetryError& err ) {
            Logger::get_mutable_instance().smslogerr( "Temporary error happened: " + boost::diagnostic_information( err ) );
        } catch ( std::runtime_error& err ) {
            Logger::get_mutable_instance().smslogerr( string("Unknown error happened: ") + err.what() );
        }
    }
}

void RequestTracker::OutboxEventLoop( ) {

    while (true) {
        try {

            SMSRequest::PTR req;
            SMSMessage::ID msgid;
            while ( true ) {
                {
                    TOutBoxQueue::Lock lck_out( *out_queue );
                    if ( out_queue->isEmpty() )
                        break;

                    req = out_queue->top().first;
                    msgid = out_queue->top().second;

                    out_queue->pop();
                }
                parseMessage2kannelEvent( req, msgid );
            }

            boost::xtime xt;
            boost::xtime_get(&xt, boost::TIME_UTC);
            xt.sec++;
            boost::thread::sleep(xt);
        } catch ( FailureError& err ) {
            Logger::get_mutable_instance().smslogerr( "Fatal error happened: " + boost::diagnostic_information( err ) );
        } catch ( RetryError& err ) {
            Logger::get_mutable_instance().smslogerr( "Temporary error happened: " + boost::diagnostic_information( err ) );
        } catch ( std::runtime_error& err ) {
            Logger::get_mutable_instance().smslogerr( string("Unknown error happened: ") + err.what() );
        }
    }
}
