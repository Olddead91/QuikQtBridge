#ifndef BRIDGETCPSERVER_H
#define BRIDGETCPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QStringList>
#include <QMap>
#include <QEventLoop>
#include <QMutex>
#include <QWaitCondition>
#include "jsonprotocolhandler.h"
#include "quikqtbridge.h"

#define BRIDGE_SERVER_PROTOCOL_VERSION  1
#define FASTCALLBACK_TIMEOUT_SEC    5

class FastCallbackRequestEventLoop;
struct ConnectionData
{
    int outMsgId;
    QString peerIp;
    JsonProtocolHandler *proto;
    QMap<QString, int> callbackSubscriptions;
    int peerProtocolVersion;
    bool versionSent;
    QList<int> objRefs;
    FastCallbackRequestEventLoop *fcbWaitResult;
    ConnectionData()
        : outMsgId(0),
          proto(nullptr),
          peerProtocolVersion(0),
          versionSent(false),
          fcbWaitResult(nullptr)
    {}
    ~ConnectionData();
};
Q_DECLARE_METATYPE(ConnectionData*)

struct ParamSubs
{
    QString param;
    QVariant value;
    QMap<ConnectionData *, int> consumers;
    QMutex mutex;
    ParamSubs(QString pname) : param(pname){}
    void addConsumer(ConnectionData *cd, int id);
    void delConsumer(ConnectionData *cd);
};

struct SecSubs
{
    QString secName;
    QMap<QString, ParamSubs *> params;
    QMutex mutex;
    SecSubs(QString sec) : secName(sec){}
    ~SecSubs();
    void addConsumer(ConnectionData *cd, QString param, int id);
    void delConsumer(ConnectionData *cd, QString param);
    void clearConsumerSubscriptions(ConnectionData *cd);
    ParamSubs *findParamSubscriptions(QString param);
};

struct ClsSubs
{
    QString className;
    QMap<QString, SecSubs *> securities;
    QMutex mutex;
    ClsSubs(QString cls) : className(cls){}
    ~ClsSubs();
    void addConsumer(ConnectionData *cd, QString sec, QString param, int id);
    void delConsumer(ConnectionData *cd, QString sec, QString param);
    void clearConsumerSubscriptions(ConnectionData *cd);
    ParamSubs *findParamSubscriptions(QString sec, QString param);
    SecSubs *findSecuritySubscriptions(QString sec);
};

class ParamSubscriptionsDb
{
public:
    ParamSubscriptionsDb();
    ~ParamSubscriptionsDb();
    void addConsumer(ConnectionData *cd, QString cls, QString sec, QString param, int id);
    void delConsumer(ConnectionData *cd, QString cls, QString sec, QString param);
    void clearConsumerSubscriptions(ConnectionData *cd);
    ParamSubs *findParamSubscriptions(QString cls, QString sec, QString param);
    SecSubs *findSecuritySubscriptions(QString cls, QString sec);
private:
    QMutex mutex;
    QMap<QString, ClsSubs *> classes;
};

class BridgeTCPServer : public QTcpServer, public QuikCallbackHandler
{
    Q_OBJECT
public:
    BridgeTCPServer(QObject *parent = nullptr);
    ~BridgeTCPServer();
    void setAllowedIPs(const QStringList &aips);

    virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres);
    virtual void fastCallbackRequest(void *data, const QVariantList &args, QVariant &res);
    virtual void clearFastCallbackData(void *data);
    virtual void sendStdoutLine(QString line);
    virtual void sendStderrLine(QString line);
private:
    QStringList m_allowedIps;
    bool ipAllowed(QString ip);
    QList<ConnectionData *> m_connections;
    QStringList activeCallbacks;

    //cache
    QStringList secClasses;
    void cacheSecClasses();

    ParamSubscriptionsDb paramSubscriptions;

    ConnectionData *getCDByProtoPtr(JsonProtocolHandler *p);
    void sendError(ConnectionData *cd, int id, int errcode, QString errmsg, bool log=false);

    void processExtendedRequests(ConnectionData *cd, int id, QString method, QJsonObject &jobj);
    void processLoadAccountsRequest(ConnectionData *cd, int id, QJsonObject &jobj);
    void processLoadClassesRequest(ConnectionData *cd, int id, QJsonObject &jobj);
    void processLoadClassSecuritiesRequest(ConnectionData *cd, int id, QJsonObject &jobj);
    void processSubscribeParamChangesRequest(ConnectionData *cd, int id, QJsonObject &jobj);
    void processUnsubscribeParamChangesRequest(ConnectionData *cd, int id, QJsonObject &jobj);
    void processExtendedAnswers(ConnectionData *cd, int id, QString method, QJsonObject &jobj);
protected:
    virtual void incomingConnection(qintptr handle);
private slots:
    void connectionEstablished(ConnectionData *cd);
    void protoReqArrived(int id, QJsonValue data);
    void protoAnsArrived(int id, QJsonValue data);
    void protoVerArrived(int ver);
    void protoEndArrived();
    void protoFinished();
    void protoError(QAbstractSocket::SocketError err);
    void serverError(QAbstractSocket::SocketError err);
    void debugLog(QString msg);

    void fastCallbackRequest(ConnectionData *cd, QString fname, QVariantList args);

    void secParamsUpdate(QString cls, QString sec);
signals:
    void fastCallbackRequestSent(ConnectionData *cd, QString fname, int id);
    void fastCallbackReturnArrived(ConnectionData *cd, int id, QVariant res);
};

class FastCallbackRequestEventLoop
{
public:
    FastCallbackRequestEventLoop(ConnectionData *rcd, QString rfname);
    QVariant sendAndWaitResult(BridgeTCPServer *server, const QVariantList &args);

    void fastCallbackRequestSent(ConnectionData *acd, QString afname, int aid);
    void fastCallbackReturnArrived(ConnectionData *acd, int aid, QVariant res);
    void connectionDataDeleted(ConnectionData *dcd);
private:
    ConnectionData *cd;
    QString funName;
    int id;
    QVariant result;
    QMutex *waitMux;
};

#endif // BRIDGETCPSERVER_H
