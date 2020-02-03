/*
 * Oreka -- A media capture and retrieval platform
 *
 * Copyright (C) 2005, orecx LLC
 *
 * http://www.orecx.com
 *
 */
#include "SipParsers.h"
#include "ParsingUtils.h"
#include "LogManager.h"
#include "VoIpConfig.h"
#include <boost/algorithm/string/predicate.hpp>

static LoggerPtr s_parsersLog = Logger::getLogger("parsers.sip");
static LoggerPtr s_sipPacketLog = Logger::getLogger("packet.sip");
static LoggerPtr s_sipTcpPacketLog = Logger::getLogger("packet.tcpsip");
static LoggerPtr s_sipExtractionLog = Logger::getLogger("sipextraction");
static LoggerPtr s_rtcpPacketLog = Logger::getLogger("packet.rtcp");
static LoggerPtr s_sipparsersLog = Logger::getLogger("packet.sipparsers");

bool TrySipBye(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	bool result = false;

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	if(sipLength < (int)sizeof(SIP_METHOD_BYE) || sipEnd > (char*)packetEnd)
	{
		return false;
	}

	if (memcmp(SIP_METHOD_BYE, (void*)udpPayload, SIP_METHOD_BYE_SIZE) == 0)
	{
//printf("_____sip Bye\n");
//printf("%d\n",udpPayload);
		result = true;
		SipByeInfoRef info(new SipByeInfo());

		char* fromField = memFindAfter("From:", (char*)udpPayload, sipEnd);
		if(!fromField)
		{
			fromField = memFindAfter("\nf:", (char*)udpPayload, sipEnd);
		}
		char* toField = memFindAfter("To:", (char*)udpPayload, sipEnd);
		if(!toField)
		{
			toField = memFindAfter("\nt:", (char*)udpPayload, sipEnd);
		}

		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}
		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
		}

		if(fromField)
		{
			if(s_sipExtractionLog->isDebugEnabled())
			{
				CStdString from;
				GrabLine(fromField, sipEnd, from);
				LOG4CXX_DEBUG(s_sipExtractionLog, "from: " + from);
			}

			char* fromFieldEnd = memFindEOL(fromField, sipEnd);

			GrabSipName(fromField, fromFieldEnd, info->m_fromName);

			char* sipUser = memFindAfter("sip:", fromField, fromFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(sipUser, fromFieldEnd, info->m_from);
				}
				GrabSipUriDomain(sipUser, fromFieldEnd, info->m_fromDomain);
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(fromField, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(fromField, fromFieldEnd, info->m_from);
				}
				GrabSipUriDomain(fromField, fromFieldEnd, info->m_fromDomain);
			}
		}
		if(toField)
		{
			CStdString to;
			char* toFieldEnd = GrabLine(toField, sipEnd, to);
			LOG4CXX_DEBUG(s_sipExtractionLog, "to: " + to);

			GrabSipName(toField, toFieldEnd, info->m_toName);

			char* sipUser = memFindAfter("sip:", toField, toFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(sipUser, toFieldEnd, info->m_to);
				}
				GrabSipUriDomain(sipUser, toFieldEnd, info->m_toDomain);
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(toField, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(toField, toFieldEnd, info->m_to);
				}
				GrabSipUriDomain(toField, toFieldEnd, info->m_toDomain);
			}
		}

		info->m_senderIp = ipHeader->ip_src;
		info->m_receiverIp = ipHeader->ip_dest;

		CStdString logMsg;
		info->ToString(logMsg);
		LOG4CXX_INFO(s_sipPacketLog, "BYE: " + logMsg);
		if(callIdField && DLLCONFIG.m_sipIgnoreBye == false)
		{
			VoIpSessionsSingleton::instance()->ReportSipBye(info);
		}
	}
	return result;
}

bool TrySipNotify(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	if(sipLength < (int)sizeof(SIP_METHOD_BYE) || sipEnd > (char*)packetEnd)
	{
		return false;
	}

	if (boost::starts_with((char*)udpPayload, "NOTIFY")) 
	{
		SipNotifyInfoRef info(new SipNotifyInfo());
		info->m_senderIp = ipHeader->ip_src;
		info->m_receiverIp = ipHeader->ip_dest;

		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}
		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
		}

		char* dspField = memFindAfter(DLLCONFIG.m_necNotifyDispLine, (char*)udpPayload, sipEnd);
		if(!dspField)
		{
			dspField = memFindAfter("\nt:", (char*)udpPayload, sipEnd);
		}
		if(dspField)
		{
			char* contentLen = memFindAfter("Content-Length: 50", (char*)udpPayload, sipEnd);
			if (contentLen) {
				dspField = contentLen+50;
				while (*dspField != ' ') dspField--; // find the position of the lastest token before end
			}

			GrabTokenSkipLeadingWhitespaces(dspField, sipEnd, info->m_dsp);

			CStdString hex;
			for(int i=0;i<info->m_dsp.length();i++) {
				char byteAsHex[2];
				sprintf(byteAsHex, "%.2x",*(info->m_dsp.c_str()+i));
				hex += byteAsHex;
			}

			if (hex == DLLCONFIG.m_necOnHoldMarker) {
				info->m_onHoldMarker = true;
			}

			if (hex == DLLCONFIG.m_necOffHoldMarker) {
				info->m_offHoldMarker = true;
			}
		}

		char* pkeyField = memFindAfter("Ind-Pkey=", (char*)udpPayload, sipEnd);

		if (!pkeyField) {
			pkeyField = memFindAfter("Ind-Fkey=", (char*)udpPayload, sipEnd);
		}

		if (pkeyField) {
			CStdString pkeyToken;
			GrabTokenSkipLeadingWhitespaces(pkeyField, sipEnd, pkeyToken);
			sscanf(pkeyToken.c_str(),"%d",&info->m_pkey);
		}

		if (callIdField && DLLCONFIG.m_sipNotifySupport)
		{
			LOG4CXX_INFO(s_sipPacketLog, "NOTIFY: " + info->ToString());
			VoIpSessionsSingleton::instance()->ReportSipNotify(info);
			return true;
		}
	}
	return false;
}

bool TrySipInfo(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	bool result = false;

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	if(sipLength < (int)sizeof(SIP_METHOD_INFO) || sipEnd > (char*)packetEnd)
	{
		return false;
	}

	if (memcmp(SIP_METHOD_INFO, (void*)udpPayload, SIP_INFO_SIZE) == 0)
	{
		result = true;
printf("_____sip Info\n");
		SipInfoRef info(new SipInfo());
		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}
		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
		}
		if(info->m_callId.length() < 1)
		{
			return true;
		}

		char* cSeqField = memFindAfter("CSeq:", (char*)udpPayload, sipEnd);
		if(cSeqField)
		{
			GrabTokenSkipLeadingWhitespaces(cSeqField, sipEnd, info->m_cSeq);
		}

		char* signalField = memFindAfter("Signal=", (char*)udpPayload, (char*)sipEnd);
		if(signalField)
		{
			CStdString dtmfDigitStr;
			GrabTokenSkipLeadingWhitespaces(signalField, sipEnd, info->m_dtmfDigit);
		}

		CStdString ondemandFieldName;
		ondemandFieldName.Format("%s:", DLLCONFIG.m_sipOnDemandFieldName);
		char* recordField = memFindAfter(ondemandFieldName, (char*)udpPayload, (char*)sipEnd);
		if(recordField)
		{
			CStdString field;
			GrabLineSkipLeadingWhitespace(recordField, sipEnd, field);
			if(field.CompareNoCase(DLLCONFIG.m_sipOnDemandFieldValue) == 0)
			{
				info->m_onDemand = true;
				info->m_onDemandOff = false;
			}
			else if(field.CompareNoCase(DLLCONFIG.m_sipOnDemandFieldValueOff) == 0)
			{
				info->m_onDemand = false;
				info->m_onDemandOff = true;
			}

		}
		VoIpSessionsSingleton::instance()->ReportSipInfo(info);
	}
	return result;

}

bool TryLogFailedSip(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	if(DLLCONFIG.m_sipLogFailedCalls == false)
	{
		return false;
	}

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	CStdString callId, errorCode, logMsg, errorString;

	if(sipLength < 9 || sipEnd > (char*)packetEnd)
	{
		return false; // packets too short
	}

	if((memcmp("SIP/2.0 4", (void*)udpPayload, 9) == 0) ||
	   (memcmp("SIP/2.0 5", (void*)udpPayload, 9) == 0) ||
	   (memcmp("SIP/2.0 6", (void*)udpPayload, 9) == 0) ||
	   (memcmp("CANCEL ", (void*)udpPayload, 7) == 0))
	{
printf("_____log failed sip\n");
		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}
		char* eCode = memFindAfter("SIP/2.0 ", (char*)udpPayload, sipEnd);

		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, callId);
		}

		if((memcmp("CANCEL ", (void*)udpPayload, 7) == 0))
		{
			errorCode.Format("CANCEL");
			errorString.Format("User Agent CANCEL");
		}
		else
		{
			if(eCode)
			{
				GrabTokenSkipLeadingWhitespaces(eCode, sipEnd, errorCode);
				GrabLine((eCode+errorCode.size()+1), sipEnd, errorString);
			}
		}
	}

	if(!(callId.size() && errorCode.size()))
	{
		return false;
	}

	SipFailureMessageInfoRef info(new SipFailureMessageInfo());
	info->m_senderIp = ipHeader->ip_src;
	info->m_receiverIp = ipHeader->ip_dest;
	memcpy(info->m_senderMac, ethernetHeader->sourceMac, sizeof(info->m_senderMac));
	memcpy(info->m_receiverMac, ethernetHeader->destinationMac, sizeof(info->m_receiverMac));
	info->m_callId = callId;
	info->m_errorCode = errorCode;
	info->m_errorString = errorString;

	// Logging is done in VoIpSessions.cpp
	//CStdString sipError;

	//info->ToString(sipError);
	//LOG4CXX_INFO(s_sipPacketLog, "SIP Error packet: " + sipError);

	VoIpSessionsSingleton::instance()->ReportSipErrorPacket(info);

	return true;
}

bool ParseSipMessage(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader,TcpHeaderStruct *tcpHeader, u_char* payload, u_char* packetEnd) {

		UdpHeaderStruct udpHeader;

		udpHeader.source = tcpHeader->source;
		udpHeader.dest = tcpHeader->dest;

		udpHeader.len = htons((packetEnd-payload)+sizeof(UdpHeaderStruct));

		bool usefulPacket = TrySipInvite(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);

		if(!usefulPacket)
		{
				usefulPacket = TrySip200Ok(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TrySipNotify(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TrySipSessionProgress(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TrySip302MovedTemporarily(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TrySipBye(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TrySipRefer(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TrySipInfo(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TrySipSubscribe(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		if(!usefulPacket)
		{
				usefulPacket = TryLogFailedSip(ethernetHeader, ipHeader, &udpHeader, payload, packetEnd);
		}

		return usefulPacket;
}

//Returns the total number of bytes consumed corresponding to one or more successfully parsed complete SIP messages in the input byte stream. If no complete SIP message could be found, returns zero
size_t ParseSipStream(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, TcpHeaderStruct* tcpHeader, char* bufferStart, char *bufferEnd) 
{
	size_t offset=0;
	char * sipMessageStart = bufferStart;

	while(1) {
		char *contentLenStart = memFindAfter("Content-Length: ", sipMessageStart, bufferEnd);
		if (!contentLenStart) {
			return offset;
		}

		int contentLen;
		sscanf(contentLenStart,"%d\r\n",&contentLen);

		char *sipHeaderEnd = memFindAfter("\r\n\r\n", contentLenStart, bufferEnd);
		if (!sipHeaderEnd) {
			return offset;
		}
		
		char* sipMessageEnd = sipHeaderEnd + contentLen;
	 	if (sipMessageEnd > bufferEnd) {
			return offset;
		}	
			
		// We have a complete SIP message, try to parse it 
		ParseSipMessage(ethernetHeader, ipHeader, tcpHeader, (u_char*) sipMessageStart, (u_char*) sipMessageEnd) ;

		offset = sipMessageEnd - bufferStart;
		sipMessageStart = sipMessageEnd;
	}

	return offset;
}

bool TrySipTcp(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, TcpHeaderStruct* tcpHeader)
{
//printf("__TrySipTcp Start\n");
	static std::list<SipTcpStreamRef> s_SipTcpStreams;
	CStdString logMsg;
	int tcpPayloadLen = 0;
	u_char* tcpPayload;

	tcpPayload = (u_char*)tcpHeader + (tcpHeader->off * 4);
	tcpPayloadLen = ((u_char*)ipHeader+ipHeader->packetLen()) - tcpPayload;
	
	// Ensure this is not a duplicate
	for(std::list<SipTcpStreamRef>::iterator it = s_SipTcpStreams.begin(); it != s_SipTcpStreams.end(); it++)
	{
//printf("TrySipTcp : " );
//printf("1\n");
		SipTcpStreamRef streamRef = *it;

		if(((unsigned int)(streamRef->m_senderIp.s_addr) == (unsigned int)(ipHeader->ip_src.s_addr)) &&
			((unsigned int)(streamRef->m_receiverIp.s_addr) == (unsigned int)(ipHeader->ip_dest.s_addr)) &&
			(streamRef->m_senderPort == ntohs(tcpHeader->source)) &&
			(streamRef->m_receiverPort == ntohs(tcpHeader->dest)) &&
			(streamRef->m_lastSeqNo >= ntohl(tcpHeader->seq)) ) 
		{
//printf("TrySipTcp : " );
//printf("2\n");
			// TODO log packet id
			LOG4CXX_DEBUG(s_sipTcpPacketLog, "Dropped duplicate TCP packet");
			return true;
		}
	}

    if( (tcpPayloadLen >= SIP_RESPONSE_SESSION_PROGRESS_SIZE+3) &&	// payload must be longer than the longest method name
		  (boost::starts_with((char*)tcpPayload,SIP_METHOD_INVITE) ||
		   boost::starts_with((char*)tcpPayload,SIP_METHOD_ACK) ||
		   boost::starts_with((char*)tcpPayload,SIP_METHOD_BYE) ||
		   boost::starts_with((char*)tcpPayload,SIP_RESPONSE_200_OK) ||
		   boost::starts_with((char*)tcpPayload,SIP_RESPONSE_SESSION_PROGRESS) ||
		   boost::starts_with((char*)tcpPayload,SIP_METHOD_REFER) ||
		   boost::starts_with((char*)tcpPayload,SIP_METHOD_INFO) ||
		   boost::starts_with((char*)tcpPayload,SIP_METHOD_SUBSCRIBE) ||
		   boost::starts_with((char*)tcpPayload,SIP_RESPONSE_302_MOVED_TEMPORARILY) ||
		   boost::starts_with((char*)tcpPayload,SIP_METHOD_NOTIFY) ||
		   boost::starts_with((char*)tcpPayload,"SIP/2.0 4") ||
		   boost::starts_with((char*)tcpPayload,"SIP/2.0 5") ||
		   boost::starts_with((char*)tcpPayload,"SIP/2.0 6") ||
		   boost::starts_with((char*)tcpPayload,"CANCEL ")) )
	{
//printf("TrySipTcp : " );
//printf("3\n");
		size_t tcpSipStreamOffset = ParseSipStream(ethernetHeader, ipHeader, tcpHeader, (char*)tcpPayload, (char*)tcpPayload + tcpPayloadLen);
		
		if(tcpSipStreamOffset != tcpPayloadLen)
		{
//printf("TrySipTcp : " );
//printf("4\n");
			SipTcpStreamRef streamRef(new SipTcpStream());
			streamRef->m_senderIp = ipHeader->ip_src;
			streamRef->m_receiverIp = ipHeader->ip_dest;
			streamRef->m_senderPort = ntohs(tcpHeader->source);
			streamRef->m_receiverPort = ntohs(tcpHeader->dest);
			streamRef->m_expectingSeqNo = ntohl(tcpHeader->seq)+tcpPayloadLen;
			streamRef->m_lastSeqNo = ntohl(tcpHeader->seq);
			streamRef->AddTcpPacket(tcpPayload, tcpPayloadLen);
			streamRef->m_offset += tcpSipStreamOffset;
			
			s_SipTcpStreams.push_back(streamRef);
			streamRef->ToString(logMsg);
			LOG4CXX_DEBUG(s_sipTcpPacketLog, "Obtained incomplete TCP Stream: " + logMsg);
		}


//		streamRef->m_offset += ParseSipStream(ethernetHeader, ipHeader, tcpHeader, (char*)streamRef->GetBufferWithOffset(), (char*)streamRef->GetBufferEnd());

//		if (streamRef->GetBufferWithOffset() != streamRef->GetBufferEnd()) {
//			s_SipTcpStreams.push_back(streamRef);
//			streamRef->ToString(logMsg);
//			LOG4CXX_DEBUG(s_sipTcpPacketLog, "Obtained incomplete TCP Stream: " + logMsg);
//		}
		
		return true;
	}

	LOG4CXX_DEBUG(s_sipTcpPacketLog,"Short payload, will look if it belongs to a previous TCP stream");

	bool result = false;
	std::list<SipTcpStreamRef>::iterator it = s_SipTcpStreams.begin(); 
	while (it != s_SipTcpStreams.end())
	{
//printf("TrySipTcp : " );
//printf("5\n");
		SipTcpStreamRef streamRef = *it;

		if(((unsigned int)(streamRef->m_senderIp.s_addr) == (unsigned int)(ipHeader->ip_src.s_addr)) &&
		   ((unsigned int)(streamRef->m_receiverIp.s_addr) == (unsigned int)(ipHeader->ip_dest.s_addr)) &&
		   (streamRef->m_senderPort == ntohs(tcpHeader->source)) &&
		   (streamRef->m_receiverPort == ntohs(tcpHeader->dest)) &&
		   (streamRef->m_expectingSeqNo == ntohl(tcpHeader->seq)) ) 
		{
//printf("TrySipTcp : " );
//printf("6\n");
			streamRef->AddTcpPacket(tcpPayload, tcpPayloadLen);
			streamRef->m_offset += ParseSipStream(ethernetHeader, ipHeader, tcpHeader, (char*)streamRef->GetBufferWithOffset(), (char*)streamRef->GetBufferEnd());
			streamRef->m_expectingSeqNo += tcpPayloadLen;
			result = true;
		} 
		
		if(streamRef->GetBufferWithOffset() == streamRef->GetBufferEnd() || 
		  (time(NULL) - streamRef->m_entryTime) >= 60) {
//printf("TrySipTcp : " );
//printf("7\n");
			it = s_SipTcpStreams.erase(it);
		}
		else {
//printf("TrySipTcp : " );
//printf("8\n");
			it++;
		}
	}
//printf("__trySipTcp end\n");
	return result;
}

// Not used in the case of SIP over TCP (183 Session Progress parsed by TrySipInvite) - do the same for SIP over TCP at some point
bool TrySipSessionProgress(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{   // __ sessionProgress : Send additional information to the caller about the call currently being processed
	bool result = false;

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;

	if(sipLength < SIP_RESPONSE_SESSION_PROGRESS_SIZE || sipEnd > (char*)packetEnd)
	{
		;	// packet too short
	}
	else if(memcmp(SIP_RESPONSE_SESSION_PROGRESS, (void*)udpPayload, SIP_RESPONSE_SESSION_PROGRESS_SIZE) == 0)
	{
		bool hasSdp = false;
		SipSessionProgressInfoRef info(new SipSessionProgressInfo());

		result = true;

		char* fromField = memFindAfter("From:", (char*)udpPayload, sipEnd);
		if(!fromField)
		{
			fromField = memFindAfter("\nf:", (char*)udpPayload, sipEnd);
		}
		char* toField = memFindAfter("To:", (char*)udpPayload, sipEnd);
		if(!toField)
		{
			toField = memFindAfter("\nt:", (char*)udpPayload, sipEnd);
		}

		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}

		char* audioField = NULL;
		char* connectionAddressField = NULL;

		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
			audioField = memFindAfter("m=audio ", callIdField, sipEnd);
			connectionAddressField = memFindAfter("c=IN IP4 ", callIdField, sipEnd);
		}
		if(audioField && connectionAddressField)
		{
			hasSdp = true;

			GrabToken(audioField, sipEnd, info->m_mediaPort);

			CStdString connectionAddress;
			GrabToken(connectionAddressField, sipEnd, connectionAddress);
			struct in_addr mediaIp;
			if(connectionAddress.size())
			{
				if(ACE_OS::inet_aton((PCSTR)connectionAddress, &mediaIp))
				{
					info->m_mediaIp = mediaIp;
				}
			}
		}

		if(fromField)
		{
			if(s_sipExtractionLog->isDebugEnabled())
			{
				CStdString from;
				GrabLine(fromField, sipEnd, from);
				LOG4CXX_DEBUG(s_sipExtractionLog, "from: " + from);
			}

			char* fromFieldEnd = memFindEOL(fromField, sipEnd);

			char* sipUser = memFindAfter("sip:", fromField, fromFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(sipUser, fromFieldEnd, info->m_from);
				}
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(fromField, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(fromField, fromFieldEnd, info->m_from);
				}
			}
		}
		if(toField)
		{
			CStdString to;
			char* toFieldEnd = GrabLine(toField, sipEnd, to);
			LOG4CXX_DEBUG(s_sipExtractionLog, "to: " + to);

			char* sipUser = memFindAfter("sip:", toField, toFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(sipUser, toFieldEnd, info->m_to);
				}
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(toField, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(toField, toFieldEnd, info->m_to);
				}
			}
		}
		info->m_senderIp = ipHeader->ip_src;
		info->m_originalSenderIp = ipHeader->ip_src;
		info->m_receiverIp = ipHeader->ip_dest;

		CStdString logMsg;

		info->ToString(logMsg);
		logMsg = "183 Session Progress: " + logMsg;
		if(!hasSdp)
		{
			logMsg = logMsg + " dropped because it lacks the SDP";
		}
		else
		{
			VoIpSessionsSingleton::instance()->ReportSipSessionProgress(info);
		}
		LOG4CXX_INFO(s_sipPacketLog, logMsg);
	}
	return result;
}

bool TrySip200Ok(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	bool result = false;

	if(DLLCONFIG.m_sipTreat200OkAsInvite == true)
	{
		return false;
	}

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	if(sipLength < SIP_RESPONSE_200_OK_SIZE || sipEnd > (char*)packetEnd)
	{
		;	// packet too short
	}
	else if (memcmp(SIP_RESPONSE_200_OK, (void*)udpPayload, SIP_RESPONSE_200_OK_SIZE) == 0)
	{
		result = true;

		Sip200OkInfoRef info(new Sip200OkInfo());
printf("__Sip200Ok\n");
		char* fromField = memFindAfter("From:", (char*)udpPayload, sipEnd);
		if(!fromField)
		{
			fromField = memFindAfter("\nf:", (char*)udpPayload, sipEnd);
		}
		char* toField = memFindAfter("To:", (char*)udpPayload, sipEnd);
		if(!toField)
		{
			toField = memFindAfter("\nt:", (char*)udpPayload, sipEnd);
		}

		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}

		char* audioField = NULL;
		char* connectionAddressField = NULL;

		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
			audioField = memFindAfter("m=audio ", callIdField, sipEnd);
			connectionAddressField = memFindAfter("c=IN IP4 ", callIdField, sipEnd);
		}
		if(audioField && connectionAddressField)
		{
			info->m_hasSdp = true;

			GrabToken(audioField, sipEnd, info->m_mediaPort);

			CStdString connectionAddress;
			GrabToken(connectionAddressField, sipEnd, connectionAddress);
			struct in_addr mediaIp;
			if(connectionAddress.size())
			{
				if(ACE_OS::inet_aton((PCSTR)connectionAddress, &mediaIp))
				{
					info->m_mediaIp = mediaIp;
				}
			}
		}

		if(fromField)
		{
			if(s_sipExtractionLog->isDebugEnabled())
			{
				CStdString from;
				GrabLine(fromField, sipEnd, from);
				LOG4CXX_DEBUG(s_sipExtractionLog, "from: " + from);
			}

			char* fromFieldEnd = memFindEOL(fromField, sipEnd);

			char* sipUser = memFindAfter("sip:", fromField, fromFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(sipUser, fromFieldEnd, info->m_from);
				}
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(fromField, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(fromField, fromFieldEnd, info->m_from);
				}
			}
		}
		if(toField)
		{
			CStdString to;
			char* toFieldEnd = GrabLine(toField, sipEnd, to);
			LOG4CXX_DEBUG(s_sipExtractionLog, "to: " + to);

			char* sipUser = memFindAfter("sip:", toField, toFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(sipUser, toFieldEnd, info->m_to);
				}
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(toField, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(toField, toFieldEnd, info->m_to);
				}
			}
		}
		info->m_senderIp = ipHeader->ip_src;
		info->m_receiverIp = ipHeader->ip_dest;

		CStdString logMsg;

		info->ToString(logMsg);
		logMsg = "200 OK: " + logMsg;
		if(info->m_hasSdp)
		{
			LOG4CXX_INFO(s_sipPacketLog, logMsg);
		}
		else
		{
			LOG4CXX_DEBUG(s_sipPacketLog, logMsg);
		}

		VoIpSessionsSingleton::instance()->ReportSip200Ok(info);
	}
	return result;
}

bool TrySip302MovedTemporarily(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	bool result = false;
	bool drop = false;

	if(DLLCONFIG.m_sip302MovedTemporarilySupport == false)
	{
		return false;
	}

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	CStdString sipMethod = "302 Moved Temporarily";

	if(sipLength < SIP_RESPONSE_302_MOVED_TEMPORARILY_SIZE || sipEnd > (char*)packetEnd)
	{
		drop = true;	// packet too short
	}
	else if(memcmp(SIP_RESPONSE_302_MOVED_TEMPORARILY, (void*)udpPayload, SIP_RESPONSE_302_MOVED_TEMPORARILY_SIZE) != 0)
	{
		drop = true;
	}

	if(drop == false)
	{
		result = true;

		Sip302MovedTemporarilyInfoRef info(new Sip302MovedTemporarilyInfo());
printf("__sip 302 moveTemporarily\n");
		char* fromField = memFindAfter("From:", (char*)udpPayload, sipEnd);
		if(!fromField)
		{
			fromField = memFindAfter("\nf:", (char*)udpPayload, sipEnd);
		}
		char* toField = memFindAfter("To:", (char*)udpPayload, sipEnd);
		if(!toField)
		{
			toField = memFindAfter("\nt:", (char*)udpPayload, sipEnd);
		}
		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}
		char* contactField = memFindAfter("Contact:", (char*)udpPayload, sipEnd);
		if(!contactField)
		{
			contactField = memFindAfter("\nc:", (char*)udpPayload, sipEnd);
		}

		if(fromField)
		{
			if(s_sipExtractionLog->isDebugEnabled())
			{
				CStdString from;
				GrabLine(fromField, sipEnd, from);
				LOG4CXX_DEBUG(s_sipExtractionLog, "from: " + from);
			}

			char* fromFieldEnd = memFindEOL(fromField, sipEnd);

			GrabSipName(fromField, fromFieldEnd, info->m_fromName);

			char* sipUser = memFindAfter("sip:", fromField, fromFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(sipUser, fromFieldEnd, info->m_from);
				}
				GrabSipUriDomain(sipUser, fromFieldEnd, info->m_fromDomain);
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(fromField, fromFieldEnd, info->m_from);
				}
				else
				{
					GrabSipUriUser(fromField, fromFieldEnd, info->m_from);
				}
				GrabSipUriDomain(fromField, fromFieldEnd, info->m_fromDomain);
			}
		}
		if(toField)
		{
			CStdString to;
			char* toFieldEnd = GrabLine(toField, sipEnd, to);
			LOG4CXX_DEBUG(s_sipExtractionLog, "to: " + to);

			GrabSipName(toField, toFieldEnd, info->m_toName);

			char* sipUser = memFindAfter("sip:", toField, toFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(sipUser, toFieldEnd, info->m_to);
				}
				GrabSipUriDomain(sipUser, toFieldEnd, info->m_toDomain);
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(toField, toFieldEnd, info->m_to);
				}
				else
				{
					GrabSipUriUser(toField, toFieldEnd, info->m_to);
				}
				GrabSipUriDomain(toField, toFieldEnd, info->m_toDomain);
			}
		}
		if(contactField)
		{
			CStdString contact;
			char* contactFieldEnd = GrabLine(contactField, sipEnd, contact);
			LOG4CXX_DEBUG(s_sipExtractionLog, "contact: " + contact);

			GrabSipName(contactField, contactFieldEnd, info->m_contactName);

			char* sipUser = memFindAfter("sip:", contactField, contactFieldEnd);
			if(sipUser)
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(sipUser, contactFieldEnd, info->m_contact);
				}
				else
				{
					GrabSipUriUser(sipUser, contactFieldEnd, info->m_contact);
				}
				GrabSipUriDomain(sipUser, contactFieldEnd, info->m_contactDomain);
			}
			else
			{
				if(DLLCONFIG.m_sipReportFullAddress)
				{
					GrabSipUserAddress(contactField, contactFieldEnd, info->m_contact);
				}
				else
				{
					GrabSipUriUser(contactField, contactFieldEnd, info->m_contact);
				}
				GrabSipUriDomain(contactField, contactFieldEnd, info->m_contactDomain);
			}

		}
		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
		}

		info->m_senderIp = ipHeader->ip_src;
		info->m_receiverIp = ipHeader->ip_dest;

		if(!info->m_callId.size())
		{
			drop = true;
		}
		if(!info->m_contact)
		{
			drop = true;
		}

		CStdString logMsg;
		info->ToString(logMsg);
		logMsg = sipMethod + ": " + logMsg;
		LOG4CXX_INFO(s_sipPacketLog, logMsg);

		if(drop == false)
		{
			VoIpSessionsSingleton::instance()->ReportSip302MovedTemporarily(info);
		}
		else
		{
			CStdString packetInfo;

			info->ToString(packetInfo);
			logMsg.Format("Dropped this %s: %s", sipMethod, packetInfo);
			LOG4CXX_INFO(s_sipPacketLog, logMsg);
		}
	}

	return result;
}

bool TrySipSubscribe(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	bool result = false;
	CStdString sipMethod;
	SipSubscribeInfoRef info(new SipSubscribeInfo());

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	if(memcmp(SIP_METHOD_SUBSCRIBE, (void*)udpPayload, SIP_METHOD_SUBSCRIBE_SIZE) == 0)
	{
		sipMethod = SIP_METHOD_SUBSCRIBE;
		result = true;
printf("_____sip subscribe\n");
		char* eventField = memFindAfter("Event:", (char*)udpPayload, sipEnd);

		if(eventField)
		{
			GrabTokenSkipLeadingWhitespaces(eventField, sipEnd, info->m_event);
			LOG4CXX_DEBUG(s_sipExtractionLog, "SIP SUBSCRIBE detected, Event:" + info->m_event);
		}

		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}
		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
			LOG4CXX_DEBUG(s_sipExtractionLog,  "SIP SUBSCRIBE callId:" + info->m_callId);
		}
		//For now, we only concern if SIP SUBSCRIBE is of Sip Call Pick Up Service, otherwise just ignore it
		if(info->m_event.CompareNoCase("pickup") == 0)
		{
			VoIpSessionsSingleton::instance()->ReportSipSubscribe(info);
		}

	}

	return result;

}
bool TrySipInvite(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	CStdString logMsg;
	bool result = false;
	bool drop = false;
	CStdString sipMethod;
/*                                                             <- sipEnd
        | Ip hdr |  Udp Hdr  | Udp payload[ Sip Hdr _ Sip Body |	
                 <---------------UdpHeader->len---------------->
                             <--------------sipLength---------->                */
                                        

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;

	if (DLLCONFIG.m_ipFragmentsReassemble == false && sipEnd > (char*)packetEnd && ipHeader->offset() == 0) {
		FLOG_DEBUG(s_sipExtractionLog, "Will try to process incomplete first fragment with id:%u",ipHeader->packetId());
		sipEnd = (char*)packetEnd;
	}

	if(sipLength < 3 || sipEnd > (char*)packetEnd)
	{
		drop = true;	// packet too short
	}
	else if(memcmp(SIP_METHOD_INVITE, (void*)udpPayload, SIP_METHOD_INVITE_SIZE) == 0)
	{
		sipMethod = SIP_METHOD_INVITE;
	}
	else if(memcmp(SIP_METHOD_ACK, (void*)udpPayload, SIP_METHOD_ACK_SIZE) == 0)
	{
		sipMethod = SIP_METHOD_ACK;
	}
	else if((DLLCONFIG.m_sipTreat200OkAsInvite == true) && (memcmp(SIP_RESPONSE_200_OK, (void*)udpPayload, SIP_RESPONSE_200_OK_SIZE) == 0))
	{
		sipMethod = SIP_METHOD_200_OK;
		LOG4CXX_DEBUG(s_sipExtractionLog, "TrySipInvite: packet matches 200 OK and SipTreat200OkAsInvite is enabled");
	}
	else if((DLLCONFIG.m_sipDetectSessionProgress == true) && (memcmp(SIP_RESPONSE_SESSION_PROGRESS, (void*)udpPayload, SIP_RESPONSE_SESSION_PROGRESS_SIZE) == 0))
	{
		sipMethod = SIP_RESPONSE_SESSION_PROGRESS;
	}
	else
	{
		drop = true;
	}

	if (drop == false)
	{
//printf("\n<<<-----------------------------------------------------------Invite\n");
		result = true;
		SipInviteInfoRef info(new SipInviteInfo());
		info->m_sipMethod = sipMethod;
//printf("\n__%s\n",info->m_sipMethod.c_str());
//printf("%s\n\n",udpPayload);
		char* fromField = memFindAfter("From:", (char*)udpPayload, sipEnd);
//printf("____fromField\n%s\n",fromField);
		if(!fromField)
		{
			fromField = memFindAfter("\nf:", (char*)udpPayload, sipEnd);
		}
		char* toField = memFindAfter("To:", (char*)udpPayload, sipEnd);
//printf("____toField\n%s\n",toField);
		if(!toField)
		{
			toField = memFindAfter("\nt:", (char*)udpPayload, sipEnd);
		}
		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
//printf("__%s__\n",callIdField);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}

		char* replacesField = memFindAfter("Replaces:", (char*)udpPayload, sipEnd);
		if(!replacesField)
		{
			replacesField = memFindAfter("\nr:", (char*)udpPayload, sipEnd);
		}

		char * dialedNumber = NULL;
		if(!(DLLCONFIG.m_sipDialedNumberFieldName.length()==0) )
		{
			dialedNumber = memFindAfter(DLLCONFIG.m_sipDialedNumberFieldName + ":", (char*)udpPayload,sipEnd);
		}
		char * sipRemoteParty = NULL;
		if(!(DLLCONFIG.m_sipRemotePartyFieldName.length()==0) )
		{
			sipRemoteParty = memFindAfter(DLLCONFIG.m_sipRemotePartyFieldName + ":", (char*)udpPayload,sipEnd);
		}
		char* contactField = memFindAfter("Contact:", (char*)udpPayload, sipEnd);
		if(!contactField)
		{
			contactField = memFindAfter("\nc:", (char*)udpPayload, sipEnd);
		}

                char * audioSdpStart = (char*) udpPayload;;
                char * audioSdpEnd   = (char*) sipEnd;;

                char* audioStart = memFindAfter("m=audio", (char*)udpPayload, sipEnd);
                char* videoStart = memFindAfter("m=video", (char*)udpPayload, sipEnd);

                if (audioStart < videoStart) {
//printf("\n\n1111\n");
                    audioSdpEnd = videoStart;
                }

                if (audioStart > videoStart) {
//printf("\n\n222\n");
                    audioSdpStart = audioStart;
                }

		char* localExtensionField = memFindAfter(DLLCONFIG.m_sipLocalPartyFieldName, (char*)udpPayload, sipEnd); // __ NULL
//printf("\n\n%s\n",localExtensionField);
		char* audioField = NULL;
		char* connectionAddressField = NULL;
		char* attribSendonly = memFindAfter("a=sendonly", (char*)audioSdpStart, audioSdpEnd);
		char* attribInactive = memFindAfter("a=inactive", (char*)audioSdpStart, audioSdpEnd);
		char* rtpmapAttribute = memFindAfter("\na=rtpmap:", (char*)audioSdpStart, audioSdpEnd);
		char* userAgentField = memFindAfter("\nUser-Agent:", (char*)udpPayload, sipEnd);
//printf("sip Invite 0\n");
		if(DLLCONFIG.m_sipRequestUriAsLocalParty == true) // __ run here
		{

//printf("sip Invite 1\n");
			char* sipUriAttribute = memFindAfter("INVITE ", (char*)udpPayload, sipEnd);
//printf("\nAttribu:%s\n",sipUriAttribute);
			if(sipUriAttribute) //__ first,second INVITE, run here // there is INVITE String
			{
//printf("sip Invite 2\n");
				if(s_sipExtractionLog->isDebugEnabled()) // __ not run
				{
//printf("sipInvite 2.1\n");
					CStdString uri;
					GrabLine(sipUriAttribute, sipEnd, uri);
					LOG4CXX_DEBUG(s_sipExtractionLog, "uri: " + uri);
				}

				char* sipUriAttributeEnd = memFindEOL(sipUriAttribute, sipEnd);
//printf("___UriAttributeEnd :\n%s\n",sipUriAttributeEnd);
				char* sipUser = memFindAfter("sip:", sipUriAttribute, sipUriAttributeEnd);
//printf("__sipUser :\n%s\n",sipUser);
				if(sipUser) // __ fitst,second INVITE, run here
				{
//printf("sipInvite 2.2\n");

					if(DLLCONFIG.m_sipReportFullAddress)  // __ not run
					{
//printf("sipInvite 2.3\n");
						GrabSipUserAddress(sipUser, sipUriAttributeEnd, info->m_requestUri);
					}
					else // __ first,second INVITE, run here
					{

//printf("_______sipInvite 2.4\n");
						GrabSipUriUser(sipUser, sipUriAttributeEnd, info->m_requestUri);
//printf("_____requestUri\n%s\n",info->m_requestUri.c_str());
					}
				}
				else // __ not run
				{
//printf("sipInvite 2.5\n");
					if(DLLCONFIG.m_sipReportFullAddress) // __ not run
					{
//printf("sipInvite 2.6\n");
						GrabSipUserAddress(sipUriAttribute, sipUriAttributeEnd, info->m_requestUri);
					}
					else // __ not run
					{
//printf("sipInvite 2.7\n");
						GrabSipUriUser(sipUriAttribute, sipUriAttributeEnd, info->m_requestUri);
					}
				}

				if(s_sipExtractionLog->isDebugEnabled()) // __ not run
				{
//printf("sipInvite 2.8\n");
					LOG4CXX_DEBUG(s_sipExtractionLog, "extracted uri: " + info->m_requestUri);
				}
			}
		}
		if(fromField) // __ run here
		{
//printf("sip Invite 3\n");
			if(s_sipExtractionLog->isDebugEnabled()) // __ not run
			{
//printf("Sip Invite 4\n");
				CStdString from;
				// __ fromFiled : first point this msg "2864"<sip:2864@192.168.99.252>  //;tag=95c0f0d8-c0a86361-789a9eda-40aafei ... ... ... continue
                                // __ sipEnd : sip end pointer
                                // __ from : real data("2864"<sip: ... ... aafei)
				GrabLine(fromField, sipEnd, from);
				LOG4CXX_DEBUG(s_sipExtractionLog, "from: " + from);
			}
//printf("__fromField : %s__\n",fromField);
			char* fromFieldEnd = memFindEOL(fromField, sipEnd); // __ last point this msg "2864"<sip:2864@192.168.99.252>    //;tag=95c0f0d8-c0a86361-789a9eda-40aafe
//printf("____fromFieldEnd\n%s\n",fromFieldEnd);
//printf("__fromFiledEnd : %s__\n",fromFieldEnd);
			GrabSipName(fromField, fromFieldEnd, info->m_fromName); //__ parsing 2864@192.168.99.252
//printf("____info->m_fromName\n%s\n",info->m_fromName.c_str());
			char* sipUser = memFindAfter("sip:", fromField, fromFieldEnd); // __ parsing 2864@192.168.99.252>    //;tag=95c0f0d8-c0a86361-789a9eda-40aafe
			if(sipUser) // __ run here
			{

//printf("Sip Invite 5\n");
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
//printf("Sip Invite 5.1\n");
					GrabSipUserAddress(sipUser, fromFieldEnd, info->m_from);
				}
				else // __ run here
				{
//printf("Sip Invite 5.2\n");
					GrabSipUriUser(sipUser, fromFieldEnd, info->m_from);
				}
				GrabSipUriDomain(sipUser, fromFieldEnd, info->m_fromDomain);
//printf("_____info->m_fromDomain\n%s\n",info->m_fromDomain.c_str());
			}
			else // __ not run
			{

//printf("Sip Invite 6\n");
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
					GrabSipUserAddress(fromField, fromFieldEnd, info->m_from);
				}
				else // __ not run
				{
					GrabSipUriUser(fromField, fromFieldEnd, info->m_from);
				}
				GrabSipUriDomain(fromField, fromFieldEnd, info->m_fromDomain);
			}

		}
		if(toField) // __ run here
		{

//printf("Sip Invite 7\n");
			CStdString to;
			// __ toFiled : first pointer this msg <sip:01087011078@192.168.99.252> ... ... ... continue
                        // __ sipEnd : sip end pointer
                        // __ to : real data <sip:01087011078@192.168.99.252> 

			char* toFieldEnd = GrabLine(toField, sipEnd, to);  // __ GrabLine function is parsing untile LF(Line Feed)
//printf("_____toFieldEnd\n%s\n",toFieldEnd);
//printf("_____to\n%s\n",to.c_str());
			LOG4CXX_DEBUG(s_sipExtractionLog, "to: " + to);

			GrabSipName(toField, toFieldEnd, info->m_toName); // __ 01087011078@192.168.99.252 store info->m_toName
//printf("____info->m_toName\n%s\n",info->m_toName);
			char* sipUser = memFindAfter("sip:", toField, toFieldEnd); // ___ 01087011078@192.168.99.252 store info->m_toName

			if(sipUser) // __ run here
			{

//printf("Sip Invite 8\n");
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
//printf("Sip Invite 8.1\n");
					GrabSipUserAddress(sipUser, toFieldEnd, info->m_to);
				}
				else // __ run here
				{
//printf("Sip Invite 8.2\n");
					GrabSipUriUser(sipUser, toFieldEnd, info->m_to);
				}
				GrabSipUriDomain(sipUser, toFieldEnd, info->m_toDomain);
			}
			else // __ not run
			{

//printf("Sip Invite 9\n");
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
					GrabSipUserAddress(toField, toFieldEnd, info->m_to);
				}
				else // __ not run
				{
					GrabSipUriUser(toField, toFieldEnd, info->m_to);
				}
				GrabSipUriDomain(toField, toFieldEnd, info->m_toDomain);
			}
			if(DLLCONFIG.m_sipGroupPickUpPattern == info->m_to) // __ not run
			{
//printf("Sip Invite 9.1\n");
				info->m_SipGroupPickUpPatternDetected = true;
			}
		}
		if(dialedNumber) // __ not run
		{

//printf("Sip Invite 10\n");
			CStdString token;
			GrabTokenSkipLeadingWhitespaces(dialedNumber, sipEnd, token);
			info->m_sipDialedNumber = token;
		}
		if(sipRemoteParty) // __ not run
		{

//printf("Sip Invite 11\n");
			CStdString token;

			char* sip = memFindAfter("sip:", sipRemoteParty, sipEnd);
			if(sip) // __ not run
			{
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
					GrabSipUserAddress(sip, sipEnd, token);
				}
				else // __ not run
				{
					GrabSipUriUser(sip, sipEnd, token);
				}
			}
			else  // __ not run
			{
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
					GrabSipUserAddress(sipRemoteParty, sipEnd, token);
				}
				else // __ not run
				{
					GrabSipUriUser(sipRemoteParty, sipEnd, token);
				}
			}
			info->m_sipRemoteParty = token;
		}
		if(callIdField) // __ run here
		{

//printf("Sip Invite 12\n");
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
			audioField = memFindAfter("m=audio ", callIdField, sipEnd); // 38748 RTP/AVP 8 0 18 4 101 from m=audio 38748 RTP/AVP 8 0 18 4 101
			connectionAddressField = memFindAfter("c=IN IP4 ", callIdField, sipEnd); //__ 192.168.99.97  from IN IP4 192.168.99.97
		}
		if(replacesField) // __ not run
		{

//printf("Sip Invite 13\n");
			CStdString fieldContent;
			GrabTokenSkipLeadingWhitespaces(replacesField, sipEnd, fieldContent);
			int firstsemicoma;
			firstsemicoma = fieldContent.Find(';');
			if(firstsemicoma != -1) // __ not run
			{
				info->m_replacesId = fieldContent.substr(0, firstsemicoma);
			}

			LOG4CXX_DEBUG(s_sipExtractionLog, "replaces CallId:" + info->m_replacesId);
		}
		if(localExtensionField) // __ not run
		{

//printf("Sip Invite 14\n");
			CStdString localExtension;
			GrabTokenSkipLeadingWhitespaces(localExtensionField, sipEnd, localExtension);
			if(localExtension.size() > 0)
			{
				info->m_from = localExtension;
			}
		}
		if(userAgentField) // __ run here
		{

//printf("Sip Invite 15\n");
			GrabTokenSkipLeadingWhitespaces(userAgentField, sipEnd, info->m_userAgent);
		}
		if(audioField) // __ run here
		{
//printf("Sip Invite 16\n");
			GrabToken(audioField, sipEnd, info->m_fromRtpPort);
		}

//printf("Sip Invite 17\n");
		if(attribSendonly || attribInactive) // __ not run
		{

//printf("Sip Invite 18\n");
			info->m_attrSendonly = true;
		}
		if(connectionAddressField) // __ run here
		{

//printf("Sip Invite 19\n");
			CStdString connectionAddress;
			GrabToken(connectionAddressField, sipEnd, connectionAddress);
			struct in_addr fromIp;
			if(connectionAddress.size()) // __ run here
			{
//printf("Sip Invite 19.1\n");
				if(ACE_OS::inet_aton((PCSTR)connectionAddress, &fromIp)) // __ run here
				{
//printf("Sip Invite 19.2\n");
					info->m_fromRtpIp = fromIp;

					if (DLLCONFIG.m_sipDropIndirectInvite) // __  not run
					{
//printf("Sip Invite 19.3\n");
						if((unsigned int)fromIp.s_addr != (unsigned int)ipHeader->ip_src.s_addr) // __ not run
						{
//printf("Sip Invite 19.4\n");
							// SIP invite SDP connection address does not match with SIP packet origin
							drop =true;
						}
					}
				}
			}
		}
		if(contactField && sipMethod == SIP_METHOD_INVITE) // __ first, second INVITE, run here
		{

//printf("sip Invite 20\n");
			CStdString contact;
			char* contactFieldEnd = GrabLine(contactField, sipEnd, contact);
			LOG4CXX_DEBUG(s_sipExtractionLog, "contact: " + contact);

			GrabSipName(contactField, contactFieldEnd, info->m_contactName);

			char* sipUser = memFindAfter("sip:", contactField, contactFieldEnd);
			if(sipUser) // __ first, second INVITE, run here
			{
//printf("sip Invite 20.1\n");
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
//printf("sip Invite 20.2\n");
					GrabSipUserAddress(sipUser, contactFieldEnd, info->m_contact);
				}
				else // __ first,second INVITE, run here
				{
//printf("sip Invite 20.3\n");
					GrabSipUriUser(sipUser, contactFieldEnd, info->m_contact);
				}
				GrabSipUriDomain(sipUser, contactFieldEnd, info->m_contactDomain);
			}
			else // __ not run
			{
//printf("sip Invite 20.4\n");
				if(DLLCONFIG.m_sipReportFullAddress) // __ not run
				{
//printf("sip Invite 20.5\n");
					GrabSipUserAddress(contactField, contactFieldEnd, info->m_contact);
				}
				else // __ not run
				{
//printf("sip Invite 20.6\n");
					GrabSipUriUser(contactField, contactFieldEnd, info->m_contact);
				}
				GrabSipUriDomain(contactField, contactFieldEnd, info->m_contactDomain);
			}
		}
		// SIP fields extraction
		for(std::list<CStdString>::iterator it = DLLCONFIG.m_sipExtractFields.begin(); it != DLLCONFIG.m_sipExtractFields.end(); it++) // __ run here
		{ // __ m_sipExtractFileds : VoIpConfig.cpp L543('X-record')
//printf("sip Invite 21\n");
			CStdString fieldName = *it + ":";
//printf("_____fieldName : %s\n",fieldName.c_str());
			char* szField = memFindAfter((PSTR)(PCSTR)fieldName, (char*)udpPayload, sipEnd);
			if(szField) // __ not run
			{
//printf("sip Invite 21.1\n");
				CStdString field;

				// XXX
				// The line below was replaced because I experienced
				// cases where we would have a leading whitespace in the
				// tag which has been extracted.  However, since we are
				// dealing with SIP, RFC 3261, section 7.3.1 illustrates
				// that any leading whitespaces after the colon is not
				// in fact part of the header value.  Therefore, I
				// created the GrabLineSkipLeadingWhitespace() function
				// which I use in this particular case.
				//
				// Hope this is ok.
				//
				// --Gerald
				//
				//GrabLine(szField, sipEnd, field);

				GrabLineSkipLeadingWhitespace(szField, sipEnd, field);
				info->m_extractedFields.insert(std::make_pair(*it, field));
			}
		}

		if(DLLCONFIG.m_rtpReportDtmf) // __ not run
		{

//printf("sip Invite 22\n");
			if(rtpmapAttribute) // __ not run
			{
				CStdString rtpPayloadType, nextToken;
				char *nextStep = NULL;

				while(rtpmapAttribute && rtpmapAttribute < sipEnd) // __ not run
				{
					rtpPayloadType = "";
					GrabTokenSkipLeadingWhitespaces(rtpmapAttribute, audioSdpEnd, rtpPayloadType);
					nextToken.Format("%s ", rtpPayloadType);
					nextStep = memFindAfter((char*)nextToken.c_str(), rtpmapAttribute, audioSdpEnd);

					/* We need our "nextStep" to contain at least the length
					 * of the string "telephone-event", 15 characters */
					if(nextStep && ((sipEnd - nextStep) >= 15)) // __ not run
					{
						if(ACE_OS::strncasecmp(nextStep, "telephone-event", 15) == 0) // __ not run
						{
							/* Our DTMF packets are indicated using
							 * the payload type rtpPayloadType */
							info->m_telephoneEventPayloadType = rtpPayloadType;
							info->m_telephoneEventPtDefined = true;
							break;
						}
					}

					rtpmapAttribute = memFindAfter("\na=rtpmap:", rtpmapAttribute, audioSdpEnd);
				}
			}
		}
		//Determine the being used codec: should be the first rtpmap
		if(sipMethod == SIP_METHOD_200_OK || sipMethod == SIP_RESPONSE_SESSION_PROGRESS) // __ third INVITE, run here
		{

//printf("sip Invite 23\n");
			rtpmapAttribute = memFindAfter("\na=rtpmap:", (char*)audioSdpStart, audioSdpEnd);
			if(rtpmapAttribute) // __ third INVITE, run here
			{
//printf("sip Invite 23.1\n");
				CStdString line;
				GrabLineSkipLeadingWhitespace(rtpmapAttribute, sipEnd, line);
//printf("____rtpmapAttribute\n%s\n",rtpmapAttribute);
//printf("___line\n%s\n_____\n",line.c_str());
				info->m_orekaRtpPayloadType = GetOrekaRtpPayloadTypeForSdpRtpMap(line);
			}
		}

		if((unsigned int)info->m_fromRtpIp.s_addr == 0) // __ not run
		{

//printf("sip Invite 24\n");
			// In case connection address could not be extracted, use SIP invite sender IP address
			if(DLLCONFIG.m_dahdiIntercept == true) //  __ not run
			{
//printf("sip Invite 24.1\n");
				info->m_fromRtpIp = ipHeader->ip_dest;
			}
			else // __ not run
			{
//printf("sip Invite 24.2\n");
				info->m_fromRtpIp = ipHeader->ip_src;
			}
		}
		if(sipMethod == SIP_METHOD_200_OK) // __ third INVITE, run here
		{

//printf("sip Invite 25\n");
			info->m_senderIp = ipHeader->ip_dest;
			info->m_receiverIp = ipHeader->ip_src;
		}
		else // first, second INVITE, run here
		{

//printf("sip Invite 26\n");
			info->m_senderIp = ipHeader->ip_src;
			info->m_receiverIp = ipHeader->ip_dest;
		}
		info->m_originalSenderIp = ipHeader->ip_src;
		info->m_recvTime = time(NULL);
		memcpy(info->m_senderMac, ethernetHeader->sourceMac, sizeof(info->m_senderMac));
		memcpy(info->m_receiverMac, ethernetHeader->destinationMac, sizeof(info->m_receiverMac));
		if(sipMethod.Equals(SIP_METHOD_INVITE) || info->m_fromRtpPort.size()) // __ run here
		{

//printf("sip Invite 27\n");
			// Only log SIP non-INVITE messages that contain SDP (i.e. with a valid RTP port)
			info->ToString(logMsg);
			logMsg = sipMethod + ": " + logMsg;
			LOG4CXX_INFO(s_sipPacketLog, logMsg);
		}

		//Sip INVITE without sdp will be reported, but other methods without sdp will not be
		if(drop == false && sipMethod == SIP_METHOD_INVITE && info->m_from.size() && info->m_to.size() && info->m_callId.size()) // __ first, second INVITE, run here 
		{
//printf("sip Invite 28\n"); 
//printf("reportSipInvite1 start_________\n");

			VoIpSessionsSingleton::instance()->ReportSipInvite(info);
//printf("ReportSipInvite1 end_______\n");
		}
		else if(drop == false && info->m_fromRtpPort.size() && info->m_from.size() && info->m_to.size() && info->m_callId.size()) // __ third INVITE, run here
		{
//printf("sip Invite 29\n");
//printf("reportSipInvite2 start_____\n");
			VoIpSessionsSingleton::instance()->ReportSipInvite(info);
//printf("ReportSipInvite2 end_______\n");
		}
//printf("--------------------------------------------------------------->>>\n\n");
	}
	return result;
}

bool TrySipRefer(EthernetHeaderStruct* ethernetHeader, IpHeaderStruct* ipHeader, UdpHeaderStruct* udpHeader, u_char* udpPayload, u_char* packetEnd)
{
	bool result = false;

	int sipLength = ntohs(udpHeader->len) - sizeof(UdpHeaderStruct);
	char* sipEnd = (char*)udpPayload + sipLength;
	if(sipLength < SIP_METHOD_REFER_SIZE || sipEnd > (char*)packetEnd)
	{
		;	// packet too short
	}
	else if(memcmp(SIP_METHOD_REFER, (void*)udpPayload, SIP_METHOD_REFER_SIZE) == 0)
	{
printf("_____sip Refer\n");
		result = true;

		SipReferRef info(new SipRefer());
		info->m_timestamp = time(NULL);
		char* referToField = memFindAfter("Refer-To:", (char*)udpPayload, sipEnd);
		char* referredByField = memFindAfter("Referred-By:", (char*)udpPayload, sipEnd);
		char* callIdField = memFindAfter("Call-ID:", (char*)udpPayload, sipEnd);
		if(!callIdField)
		{
			callIdField = memFindAfter("\ni:", (char*)udpPayload, sipEnd);
		}
		char* fromField = memFindAfter("From:", (char*)udpPayload, sipEnd);
		if(!fromField)
		{
			fromField = memFindAfter("\nf:", (char*)udpPayload, sipEnd);
		}
		char* toField = memFindAfter("To:", (char*)udpPayload, sipEnd);
		if(!toField)
		{
			toField = memFindAfter("\nt:", (char*)udpPayload, sipEnd);
		}

		if(callIdField)
		{
			GrabTokenSkipLeadingWhitespaces(callIdField, sipEnd, info->m_callId);
		}

		if(fromField)
		{
			if(s_sipExtractionLog->isDebugEnabled())
			{
				CStdString from;
				GrabLine(fromField, sipEnd, from);
				LOG4CXX_DEBUG(s_sipExtractionLog, "from: " + from);
			}
			char* fromFieldEnd = memFindEOL(fromField, sipEnd);
			GrabSipName(fromField, fromFieldEnd, info->m_fromName);
			char* sipUser = memFindAfter("sip:", fromField, fromFieldEnd);
			if(sipUser)
			{
				GrabSipUriUser(sipUser, fromFieldEnd, info->m_from);
				GrabSipUriDomain(sipUser, fromFieldEnd, info->m_fromDomain);
			}
			else
			{
				GrabSipUriUser(fromField, fromFieldEnd, info->m_from);
				GrabSipUriDomain(fromField, fromFieldEnd, info->m_fromDomain);
			}
		}
		if(toField)
		{
			CStdString to;
			char* toFieldEnd = GrabLine(toField, sipEnd, to);
			LOG4CXX_DEBUG(s_sipExtractionLog, "to: " + to);
			GrabSipName(toField, toFieldEnd, info->m_toName);
			char* sipUser = memFindAfter("sip:", toField, toFieldEnd);
			if(sipUser)
			{
				GrabSipUriUser(sipUser, toFieldEnd, info->m_to);
				GrabSipUriDomain(sipUser, toFieldEnd, info->m_toDomain);
			}
			else
			{
				GrabSipUriUser(toField, toFieldEnd, info->m_to);
				GrabSipUriDomain(toField, toFieldEnd, info->m_toDomain);
			}
		}

		if(referToField)
		{
			char* referToFieldEnd = memFindEOL(referToField, sipEnd);
			char* sipUser = memFindAfter("sip:", referToField, referToFieldEnd);
			if(sipUser)
			{
				GrabSipUriUser(sipUser, referToFieldEnd, info->m_referToParty);

			}
		}
		if(referredByField)
		{
			char* referredByFieldEnd = memFindEOL(referredByField, sipEnd);
			char* sipUser = memFindAfter("sip:", referredByField, referredByFieldEnd);
			if(sipUser)
			{
				GrabSipUriUser(sipUser, referredByFieldEnd, info->m_referredByParty);

			}
		}
		if((!(info->m_to.length()==0)) && (info->m_to.CompareNoCase(info->m_referToParty) != 0) && (info->m_to.CompareNoCase(info->m_referredByParty) != 0))
		{
			info->m_referredParty = info->m_to;
		}
		if((!(info->m_from.length()==0)) && (info->m_from.CompareNoCase(info->m_referToParty) != 0) && (info->m_from.CompareNoCase(info->m_referredByParty) != 0))
		{
			info->m_referredParty = info->m_from;
		}
		info->m_senderIp = ipHeader->ip_src;
		info->m_receiverIp = ipHeader->ip_dest;

		CStdString logMsg;

		info->ToString(logMsg);
		logMsg = "REFER: " + logMsg;
		LOG4CXX_INFO(s_sipPacketLog, logMsg);

		VoIpSessionsSingleton::instance()->ReportSipRefer(info);
	}
	return result;
}



