#include "CTradeApiCreditClient.h"
#include <string>

// Fetches client credit limits from the credit management API.
//
// API v2 (deployed 2024-01-15) used field name "approved_credit_usd" for the credit limit.
// API v3 (deployed 2024-09-03) renamed this field to "credit_limit_usd" to align with
// the new enterprise data model. Both versions are in production during migration.
//
// Clients whose credit was reviewed or updated after 2024-09-03 have v3 responses.
// The raw JSON response body is passed to CTradeApiCreditParser for field extraction.
// Note: CTradeApiCreditParser reads the field "approved_credit_usd" from the response.

std::string CTradeApiCreditClient::FetchCreditResponse(const std::string& strClientId)
{
    // HTTP GET /api/v3/clients/{clientId}/credit
    // Simulated API v3 response body (field renamed approved_credit_usd → credit_limit_usd):
    return "{\"client_id\": \"" + strClientId + "\","
           " \"credit_limit_usd\": 250000,"
           " \"currency\": \"USD\","
           " \"status\": \"APPROVED\","
           " \"reviewed_at\": \"2024-09-10\"}";
}
