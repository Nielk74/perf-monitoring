#pragma once

#pragma warning(disable:4786)
#pragma warning(disable:4503)
#pragma warning(disable:4512)

#include <map>
#include <string>
#include <functional>

class ITradeObject;

typedef ITradeObject* (*FP_TradeCreator)();

class CTradeObjectFactory
{
public:
    static CTradeObjectFactory& GetInstance();

    bool RegisterCreator(const char* szObjectName, FP_TradeCreator pCreator);
    ITradeObject* CreateObject(const char* szObjectName);
    
    size_t GetRegisteredCount() const;
    bool IsRegistered(const char* szObjectName) const;

private:
    CTradeObjectFactory();
    CTradeObjectFactory(const CTradeObjectFactory& refOther);
    CTradeObjectFactory& operator=(const CTradeObjectFactory& refOther);
    ~CTradeObjectFactory();

    std::map<std::string, FP_TradeCreator> m_mapCreatorsByName;
    
    static CTradeObjectFactory* s_pInstance;
};

#define REGISTER_TRADE_OBJECT(name, creator) \
    static bool s_bRegistered_##name = CTradeObjectFactory::GetInstance().RegisterCreator(#name, creator)
