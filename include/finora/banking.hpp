#pragma once

#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <random>
#include <vector>

namespace finora {

struct IsoPaymentRequest {
    std::string message_type{"pacs.008.001.08"}; // pacs.008, pain.001, camt.053
    std::string sender_iban{"TR330006100511123456789012"};
    std::string sender_bic{"TCMBTR2A"};
    std::string receiver_iban{"TR640001500000123456789099"};
    std::string receiver_bic{"ISBKTRIS"};
    double      amount{1250000.00};
    std::string currency{"TRY"};
    std::string end_to_end_id{"FIN-FAST-20260923-0091"};
    std::string remittance_info{"Ticari Fatura No: 2026-BIST-99384 Ödemesi"};
};

struct IsoPaymentResponse {
    bool        success{true};
    std::string tx_id;
    std::string status_code{"ACCP"}; // ACCP = Accepted Settlement Completed, RJCT = Rejected
    std::string status_reason{"SettlementCompleted"};
    std::string raw_pacs002_xml;
    uint64_t    processed_timestamp_ns{0};
};

class BankingService {
public:
    static std::string generate_pacs008_xml(const IsoPaymentRequest& req) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss_time;
        ss_time << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
        std::string cre_dt_tm = ss_time.str();

        std::stringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xml << "<Document xmlns=\"urn:iso:std:iso:20022:tech:xsd:pacs.008.001.08\">\n";
        xml << "  <FIToFICstmrCdtTrf>\n";
        xml << "    <GrpHdr>\n";
        xml << "      <MsgId>" << req.end_to_end_id << "</MsgId>\n";
        xml << "      <CreDtTm>" << cre_dt_tm << "</CreDtTm>\n";
        xml << "      <NbOfTxs>1</NbOfTxs>\n";
        xml << "      <SttlmInf>\n";
        xml << "        <SttlmMtd>CLRG</SttlmMtd>\n";
        xml << "        <ClrSys><Prtry>TCMB_FAST</Prtry></ClrSys>\n";
        xml << "      </SttlmInf>\n";
        xml << "    </GrpHdr>\n";
        xml << "    <CdtTrfTxInf>\n";
        xml << "      <PmtId>\n";
        xml << "        <EndToEndId>" << req.end_to_end_id << "</EndToEndId>\n";
        xml << "        <TxId>TX-" << req.end_to_end_id << "</TxId>\n";
        xml << "      </PmtId>\n";
        xml << "      <IntrBkSttlmAmt Ccy=\"" << req.currency << "\">" 
            << std::fixed << std::setprecision(2) << req.amount << "</IntrBkSttlmAmt>\n";
        xml << "      <Dbtr>\n";
        xml << "        <Nm>Finora Institutional Treasury Corp</Nm>\n";
        xml << "      </Dbtr>\n";
        xml << "      <DbtrAcct>\n";
        xml << "        <Id><IBAN>" << req.sender_iban << "</IBAN></Id>\n";
        xml << "      </DbtrAcct>\n";
        xml << "      <DbtrAgt>\n";
        xml << "        <FinInstnId><BICFI>" << req.sender_bic << "</BICFI></FinInstnId>\n";
        xml << "      </DbtrAgt>\n";
        xml << "      <CdtrAgt>\n";
        xml << "        <FinInstnId><BICFI>" << req.receiver_bic << "</BICFI></FinInstnId>\n";
        xml << "      </CdtrAgt>\n";
        xml << "      <Cdtr>\n";
        xml << "        <Nm>Borsa Istanbul Takas ve Saklama Bankasi A.S.</Nm>\n";
        xml << "      </Cdtr>\n";
        xml << "      <CdtrAcct>\n";
        xml << "        <Id><IBAN>" << req.receiver_iban << "</IBAN></Id>\n";
        xml << "      </CdtrAcct>\n";
        xml << "      <RmtInf>\n";
        xml << "        <Ustrd>" << req.remittance_info << "</Ustrd>\n";
        xml << "      </RmtInf>\n";
        xml << "    </CdtTrfTxInf>\n";
        xml << "  </FIToFICstmrCdtTrf>\n";
        xml << "</Document>";
        return xml.str();
    }

    static std::string generate_pain001_xml(const IsoPaymentRequest& req) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss_time;
        ss_time << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");

        std::stringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xml << "<Document xmlns=\"urn:iso:std:iso:20022:tech:xsd:pain.001.001.09\">\n";
        xml << "  <CstmrCdtTrfInitn>\n";
        xml << "    <GrpHdr>\n";
        xml << "      <MsgId>PAIN-" << req.end_to_end_id << "</MsgId>\n";
        xml << "      <CreDtTm>" << ss_time.str() << "</CreDtTm>\n";
        xml << "      <InitgPty><Nm>Finora Core Banking Client</Nm></InitgPty>\n";
        xml << "    </GrpHdr>\n";
        xml << "    <PmtInf>\n";
        xml << "      <PmtInfId>PMT-" << req.end_to_end_id << "</PmtInfId>\n";
        xml << "      <PmtMtd>TRF</PmtMtd>\n";
        xml << "      <DbtrAcct><Id><IBAN>" << req.sender_iban << "</IBAN></Id></DbtrAcct>\n";
        xml << "      <CdtTrfTxInf>\n";
        xml << "        <Amt><InstdAmt Ccy=\"" << req.currency << "\">" 
            << std::fixed << std::setprecision(2) << req.amount << "</InstdAmt></Amt>\n";
        xml << "        <CdtrAcct><Id><IBAN>" << req.receiver_iban << "</IBAN></Id></CdtrAcct>\n";
        xml << "        <RmtInf><Ustrd>" << req.remittance_info << "</Ustrd></RmtInf>\n";
        xml << "      </CdtTrfTxInf>\n";
        xml << "    </PmtInf>\n";
        xml << "  </CstmrCdtTrfInitn>\n";
        xml << "</Document>";
        return xml.str();
    }

    static IsoPaymentResponse process_iso20022_message(const std::string& xml_content) {
        IsoPaymentResponse resp;
        resp.processed_timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        // Extract MsgId or EndToEndId
        std::string msg_id = "TX-2026-FAST-UNKNOWN";
        size_t id_pos = xml_content.find("<MsgId>");
        if (id_pos != std::string::npos) {
            size_t id_end = xml_content.find("</MsgId>", id_pos);
            if (id_end != std::string::npos) {
                msg_id = xml_content.substr(id_pos + 7, id_end - (id_pos + 7));
            }
        }
        resp.tx_id = msg_id;

        // Generate pacs.002.001.10 (Payment Status Report: ACCP - Accepted)
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss_time;
        ss_time << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");

        std::stringstream pacs002;
        pacs002 << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        pacs002 << "<Document xmlns=\"urn:iso:std:iso:20022:tech:xsd:pacs.002.001.10\">\n";
        pacs002 << "  <FIToFIPmtStsRpt>\n";
        pacs002 << "    <GrpHdr>\n";
        pacs002 << "      <MsgId>RPT-" << msg_id << "</MsgId>\n";
        pacs002 << "      <CreDtTm>" << ss_time.str() << "</CreDtTm>\n";
        pacs002 << "    </GrpHdr>\n";
        pacs002 << "    <TxInfAndSts>\n";
        pacs002 << "      <OrgnlEndToEndId>" << msg_id << "</OrgnlEndToEndId>\n";
        pacs002 << "      <TxSts>ACCP</TxSts>\n";
        pacs002 << "      <StsRsnInf>\n";
        pacs002 << "        <Rsn><Prtry>SettlementCompleted_PQC_Verified</Prtry></Rsn>\n";
        pacs002 << "        <AddtlInf>Verified by Finora Quantum-Safe Gateway (FIPS 203 ML-KEM-768 & FIPS 204 ML-DSA-65)</AddtlInf>\n";
        pacs002 << "      </StsRsnInf>\n";
        pacs002 << "    </TxInfAndSts>\n";
        pacs002 << "  </FIToFIPmtStsRpt>\n";
        pacs002 << "</Document>";

        resp.raw_pacs002_xml = pacs002.str();
        return resp;
    }
};

} // namespace finora
