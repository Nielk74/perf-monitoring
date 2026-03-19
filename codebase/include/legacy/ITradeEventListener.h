#pragma once

#pragma warning(disable:4100)
#pragma warning(disable:4512)

class ITradeEventListener
{
public:
    virtual ~ITradeEventListener() {}

    virtual void OnTradeCreated(long nTradeId, const char* szTradeType) = 0;
    virtual void OnTradeValidated(long nTradeId, bool bIsValid, const char* szValidationMsg) = 0;
    virtual void OnTradeSubmitted(long nTradeId, const char* szSubmissionStatus) = 0;

protected:
    ITradeEventListener() {}
};
