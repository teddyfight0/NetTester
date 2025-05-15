//Nettester �Ĺ����ļ�
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
using namespace std;

//����Ϊ��Ҫ�ı���
U8* sendbuf;        //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
int printCount = 0; //��ӡ����
int spin = 0;  //��ӡ��̬��Ϣ����

//------һЩͳ���õ�ȫ�ֱ���------------
int iSndTotal = 0;  //������������
int iSndTotalCount = 0; //���������ܴ���
int iSndErrorCount = 0;  //���ʹ������
int iRcvForward = 0;     //ת����������
int iRcvForwardCount = 0; //ת�������ܴ���
int iRcvToUpper = 0;      //�ӵͲ�ݽ��߲���������
int iRcvToUpperCount = 0;  //�ӵͲ�ݽ��߲������ܴ���
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���



//��ӡͳ����Ϣ
void print_statistics();
void menu();
//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
void InitFunction(CCfgFileParms& cfgParms)
{
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL ) {
		cout << "�ڴ治��" << endl;
		exit(0);
	}
	return;
}
//***************��Ҫ��������******************************
//���ƣ�EndFunction
//���ܣ����������棬��main�������յ�exit������������˳�ǰ����
//���룺
//�����
void EndFunction()
{
	if(sendbuf != NULL)
		free(sendbuf);
	return;
}

//-----------------------------------------------------------------------------------
// ����������ip���ݰ�

// ·�ɱ���ṹ
struct RouteEntry {
	in_addr destIP;      // Ŀ��IP��ַ
	in_addr nextHop;     // ��һ��IP
	int outPort;         // ���ӿں�
	bool isValid;        // �Ƿ���Ч
};

// ȫ�ֱ���
const int MAX_ROUTE_ENTRIES = 20;  // ���·�ɱ�����
RouteEntry routeTable[MAX_ROUTE_ENTRIES];  // ·�ɱ�
int routeCount = 0;  // ��ǰ·�ɱ�����
// IP-Portӳ�����ṹ (����ARP��)
// IP-Portӳ�����ṹ (����ARP��)
struct IPPortMapping {
	in_addr ipAddr;      // IP��ַ
	int port;           // ��Ӧ�Ķ˿ں� 
	bool isValid;       // �Ƿ���Ч

	// MAC��ַ�ṹ(6�ֽ�)
	struct MacAddress {
		U8 bytes[6];    // MAC��ַ�ֽ�����
	} mac;

	// ����IP��ַ����MAC��ַ
	void generateMACFromIP() {
		// ����MAC��ַ
		memset(mac.bytes, 0, sizeof(mac.bytes));

		// ��IP��ַ(127.0.x.x��ʽ)��ȡ��Ԫ�źͶ˿ں�
		UCHAR* ipBytes = (UCHAR*)&ipAddr.S_un.S_addr;

		// ��Ԫ���ڵ�3���ֽ�(x.x.DeviceID.x)
		int devNum = ipBytes[2];  // ��ȡ��Ԫ��
		mac.bytes[0] = 0;         // ���ֽ���Ϊ0
		mac.bytes[1] = devNum;    // ���ֽ�Ϊ��Ԫ��

		// �˿ں��ڵ�4���ֽ�(x.x.x.PortID)
		int portNum = ipBytes[3]; // ��ȡ�˿ں�
		mac.bytes[2] = 0;        // ���ֽ���Ϊ0 
		mac.bytes[3] = portNum;  // ���ֽ�Ϊ�˿ں�

		// ��������ֽ��ڴ����ʱ��ѵض˿ںŽ����滻
		mac.bytes[4] = 1;
		mac.bytes[5] = 2;
	}

	// ��ȡMAC��ַ���ַ�����ʾ
	string getMACString() const {
		char macStr[18];
		snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
			mac.bytes[0], mac.bytes[1], mac.bytes[2],
			mac.bytes[3], mac.bytes[4], mac.bytes[5]);
		return string(macStr);
	}
};

// ��function.cpp��ͷ���ȫ�ֱ���
const int MAX_IP_PORT_MAPPINGS = 20;
IPPortMapping ipPortMappings[MAX_IP_PORT_MAPPINGS];
int mappingCount = 0;

// ���lookupIPPort������ʵ��
int lookupIPPort(in_addr destIP) {
	for (int i = 0; i < mappingCount; i++) {
		if (ipPortMappings[i].isValid &&
			ipPortMappings[i].ipAddr.S_un.S_addr == destIP.S_un.S_addr) {
			return ipPortMappings[i].port;
		}
	}
	return -1;
}
// IPͷ���ṹ
struct IPHeader {
	in_addr srcIP;   // ԴIP
	in_addr destIP;  // Ŀ��IP
	U8 data[0];      // ���ݲ���
};



//---------------------------------------------------------------------------------

//***************��Ҫ��������******************************
//���ƣ�TimeOut
//���ܣ�������������ʱ����ζ��sBasicTimer�����õĳ�ʱʱ�䵽�ˣ�
//      �������ݿ���ȫ���滻Ϊ������Լ����뷨
//      ������ʵ���˼���ͬʱ���й��ܣ����ο�
//      1)����iWorkMode����ģʽ���ж��Ƿ񽫼�����������ݷ��ͣ�
//        ��Ϊscanf�����������¼�ʱ���ڵȴ����̵�ʱ����ȫʧЧ������ʹ��_kbhit()������������ϵ��ڼ�ʱ�Ŀ������жϼ���״̬�������Get��û��
//      2)����ˢ�´�ӡ����ͳ��ֵ��ͨ����ӡ���Ʒ��Ŀ��ƣ�����ʼ�ձ�����ͬһ�д�ӡ��Get��
//���룺ʱ�䵽�˾ʹ�����ֻ��ͨ��ȫ�ֱ�����������
//���������Ǹ�����Ŭ���ɻ����ʵ����
void TimeOut()
{

	printCount++;
	if (_kbhit()) {
		//�����ж���������˵�ģʽ
		menu();
	}

	print_statistics();
}
//------------�����ķָ��ߣ����������ݵ��շ�,--------------------------------------------

//***************��Ҫ��������******************************
//���ƣ�RecvfromUpper
//���ܣ�������������ʱ����ζ���յ�һ�ݸ߲��·�������
//      ��������ȫ�������滻��������Լ���
//      ���̹��ܽ���
//         1)ͨ���Ͳ�����ݸ�ʽ����lowerMode���ж�Ҫ��Ҫ������ת����bit�����鷢�ͣ�����ֻ�����Ͳ�ӿ�0��
//           ��Ϊû���κοɹ��ο��Ĳ��ԣ���������Ӧ�ø���Ŀ�ĵ�ַ�ڶ���ӿ���ѡ��ת���ġ�
//         2)�ж�iWorkMode�������ǲ�����Ҫ�����͵��������ݶ���ӡ������ʱ���ԣ���ʽ����ʱ�����齫����ȫ����ӡ��
//���룺U8 * buf,�߲㴫���������ݣ� int len�����ݳ��ȣ���λ�ֽ�
//�����
void RecvfromUpper(U8* buf, int len) {
	int iSndRetval = 0;
	U8* bufSend = NULL;

	in_addr destIP;
	memcpy(&destIP, buf, sizeof(in_addr));

	int ipPacketLen = sizeof(IPHeader) + len - sizeof(in_addr);
	U8* ipPacket = (U8*)malloc(ipPacketLen);
	if (!ipPacket) {
		cout << "�ڴ治��" << endl;
		return;
	}

	IPHeader* ipHdr = (IPHeader*)ipPacket;
	ipHdr->srcIP = local_addr.sin_addr;
	ipHdr->destIP = destIP;
	memcpy(ipHdr->data, buf + sizeof(in_addr), len - sizeof(in_addr));

	int outPort = lookupIPPort(destIP);
	if (outPort < 0) {
		printf("\n[NET]δ�ҵ�IP %s�Ķ˿�ӳ��,�㲥ת��\n", inet_ntoa(destIP));
		for (int i = 0; i < lowerNumber; i++) {
			if (lowerMode[i] == 0) {
				bufSend = (U8*)malloc(ipPacketLen * 8);
				if (!bufSend) {
					free(ipPacket); // �ͷ� ipPacket
					cout << "�ڴ治��" << endl;
					return;
				}
				iSndRetval = ByteArrayToBitArray(bufSend, ipPacketLen * 8, ipPacket, ipPacketLen);
				if (iSndRetval > 0) {
					SendtoLower(bufSend, iSndRetval, i);
				}
				free(bufSend);
			}
			else {
				iSndRetval = SendtoLower(ipPacket, ipPacketLen, i);
			}
		}
	}
	else {
		printf("\n[NET]ת��IP�� %s -> %s �Ӷ˿� %d\n",
			inet_ntoa(ipHdr->srcIP), inet_ntoa(ipHdr->destIP), outPort);
		if (lowerMode[outPort] == 0) {
			bufSend = (U8*)malloc(ipPacketLen * 8);
			if (!bufSend) {
				free(ipPacket); // �ͷ� ipPacket
				cout << "�ڴ治��" << endl;
				return;
			}
			iSndRetval = ByteArrayToBitArray(bufSend, ipPacketLen * 8, ipPacket, ipPacketLen);
			if (iSndRetval > 0) {
				SendtoLower(bufSend, iSndRetval, outPort);
			}
			free(bufSend);
		}
		else {
			iSndRetval = SendtoLower(ipPacket, ipPacketLen, outPort);
		}
	}

	free(ipPacket); // ȷ���ͷ�

	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iSndTotal += iSndRetval;
		iSndTotalCount++;
	}
}

//***************��Ҫ��������******************************
//���ƣ�RecvfromLower
//���ܣ�������������ʱ����ζ�ŵõ�һ�ݴӵͲ�ʵ��ݽ�����������
//���룺U8 * buf,�Ͳ�ݽ����������ݣ� int len�����ݳ��ȣ���λ�ֽڣ�int ifNo ���Ͳ�ʵ����룬�����������ĸ��Ͳ�
//�����
void RecvfromLower(U8* buf, int len, int ifNo) {
	int iSndRetval = 0; // ��ʼ��
	U8* bufSend = NULL;

	if (len < sizeof(IPHeader)) {
		cout << "���յ�������̫��,�޷�����IPͷ" << endl;
		return;
	}

	IPHeader* ipHdr = (IPHeader*)buf;

	if (ipHdr->destIP.S_un.S_addr == local_addr.sin_addr.S_un.S_addr) {
		if (lowerMode[ifNo] == 0) {
			int byteLen = (len + 7) / 8;
			bufSend = (U8*)malloc(byteLen);
			if (bufSend == NULL) {
				cout << "�ڴ治��" << endl;
				return;
			}
			iSndRetval = BitArrayToByteArray(buf, len, bufSend, byteLen);
			if (iSndRetval > 0) {
				iSndRetval = SendtoUpper(bufSend, iSndRetval);
			}
			free(bufSend);
		}
		else {
			iSndRetval = SendtoUpper(buf, len);
		}
		if (iSndRetval > 0) {
			iRcvToUpper += iSndRetval;
			iRcvToUpperCount++;
		}
	}
	else {
		int outPort = lookupIPPort(ipHdr->destIP);
		if (outPort >= 0 && outPort != ifNo) {
			if (lowerMode[outPort] == 0) {
				bufSend = (U8*)malloc(len * 8);
				if (bufSend == NULL) {
					cout << "�ڴ治��" << endl;
					return;
				}
				iSndRetval = ByteArrayToBitArray(bufSend, len * 8, buf, len);
				if (iSndRetval > 0) {
					iSndRetval = SendtoLower(bufSend, iSndRetval, outPort);
				}
				free(bufSend);
			}
			else {
				iSndRetval = SendtoLower(buf, len, outPort);
			}
			if (iSndRetval > 0) {
				iRcvForward += iSndRetval;
				iRcvForwardCount++;
			}
		}
	}
}

void print_statistics()
{
	if (printCount % 10 == 0) {
		switch (spin) {
		case 1:
			printf("\r-");
			break;
		case 2:
			printf("\r\\");
			break;
		case 3:
			printf("\r|");
			break;
		case 4:
			printf("\r/");
			spin = 0;
			break;
		}
		cout << "��ת�� "<< iRcvForward<< " λ��"<< iRcvForwardCount<<" �Σ�"<<"�ݽ� "<< iRcvToUpper<<" λ��"<< iRcvToUpperCount<<" ��,"<<"���� "<< iSndTotal <<" λ��"<< iSndTotalCount<<" �Σ�"<< "���Ͳ��ɹ� "<< iSndErrorCount<<" ��,""�յ�������Դ "<< iRcvUnknownCount<<" �Ρ�";
		spin++;
	}

}
//PrintParms ��ӡ����������ע�ⲻ��cfgFilms�������ģ�����Ŀǰ��Ч�Ĳ���
void PrintParms()
{
	size_t i;
	cout << "�豸��: " << strDevID << " ���: " << strLayer << "ʵ��: " << strEntity << endl;
	cout << "�ϲ�ʵ���ַ: " << inet_ntoa(upper_addr.sin_addr) << "  UDP�˿ں�: " << ntohs(upper_addr.sin_port) << endl;

	cout << "����ʵ���ַ: " << inet_ntoa(local_addr.sin_addr) << "  UDP�˿ں�: " << ntohs(local_addr.sin_port) << endl;
	if (strLayer.compare("PHY") == 0) {
		if (lowerNumber <= 1) {
			cout << "�²�㵽���ŵ�" << endl;
			cout << "��·�Զ˵�ַ: ";
		}
		else {
			cout << "�²�㲥ʽ�ŵ�" << endl;
			cout << "�����ŵ�վ�㣺";
		}
	}
	else {
		cout << "�²�ʵ��";
	}
	if (lowerNumber == 1) {
		cout << "��ַ��" << inet_ntoa(lower_addr[0].sin_addr) << "  UDP�˿ں�: " << ntohs(lower_addr[0].sin_port) << endl;
	}
	else {
		if (strLayer.compare("PHY") == 0) {
			cout << endl;
			for (i = 0; i < lowerNumber; i++) {
				cout << "        ��ַ��" << inet_ntoa(lower_addr[i].sin_addr) << "  UDP�˿ں�: " << ntohs(lower_addr[i].sin_port) << endl;
			}
		}
		else {
			cout << endl;
			for (i = 0; i < lowerNumber; i++) {
				cout << "        �ӿ�: [" << i << "] ��ַ" << inet_ntoa(lower_addr[i].sin_addr) << "  UDP�˿ں�: " << ntohs(lower_addr[i].sin_port) << endl;
			}
		}
	}
	string strTmp;
	//strTmp = getValueStr("cmdIpAddr");
	cout << "ͳһ����ƽ̨��ַ: " << inet_ntoa(cmd_addr.sin_addr);
	//strTmp = getValueStr("cmdPort");
	cout << "  UDP�˿ں�: " << ntohs(cmd_addr.sin_port) << endl;
	//strTmp = getValueStr("oneTouchAddr");
	cout << "oneTouchһ��������ַ: " << inet_ntoa(oneTouch_addr.sin_addr);
	//strTmp = getValueStr("oneTouchPort");
	cout << "  UDP�˿ں�; " << ntohs(oneTouch_addr.sin_port) << endl;
	cout << "##################" << endl;
	//printArray();
	cout << "--------------------------------------------------------------------" << endl;
	cout << endl;

}
void menu()
{
	int selection;
	unsigned short port;
	int iSndRetval;
	char kbBuf[100];
	int len;
	U8* bufSend;
	//����|��ӡ��[���Ϳ��ƣ�0���ȴ��������룻1���Զ���][��ӡ���ƣ�0�������ڴ�ӡͳ����Ϣ��1����bit����ӡ���ݣ�2���ֽ�����ӡ����]
	cout << endl << endl << "�豸��:" << strDevID << ",    ���:" << strLayer << ",    ʵ���:" << strEntity ;
	printf("\n");
	cout << "8-�Զ����ݷ�װ��ת������" << endl;
	cout << endl << "0-ȡ��" << endl << "����������ѡ�����";
	cin >> selection;
	switch (selection) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		break;
	case 5:
		break;
	case 6:
		break;
	case 7:
		PrintParms();
		break;
	case 8: {
		cout << "�Զ����ݷ�װ�ͱ�����ת������:" << endl;

		// �����������
		const int testDataSize = 100; // �����С
		U8* testData = (U8*)malloc(testDataSize);
		if (!testData) {
			cout << "�ڴ治��" << endl;
			break;
		}
		int testLen = snprintf((char*)testData, testDataSize, "Test Message");
		if (testLen < 0 || testLen >= testDataSize) {
			free(testData);
			cout << "�������ݹ���ʧ��" << endl;
			break;
		}

		// ��ӡԭʼ���ݣ��ֽ�����
		cout << "ԭʼ���ݣ��ֽ�����:" << endl;
		print_data_byte(testData, testLen, 1);

		// ת��Ϊ������
		int bitLen = testLen * 8; // ÿ���ֽ�8λ
		U8* bitData = (U8*)malloc(bitLen);
		if (!bitData) {
			free(testData);
			cout << "�ڴ治��" << endl;
			break;
		}
		int bitDataLen = ByteArrayToBitArray(bitData, bitLen, testData, testLen);
		if (bitDataLen <= 0) {
			free(testData);
			free(bitData);
			cout << "�ֽڵ�����ת��ʧ��" << endl;
			break;
		}

		// ��ӡ������
		cout << "\n�������ݣ���������:" << endl;
		print_data_bit(bitData , bitDataLen ,0); // ������ڴ�ӡ�������ĺ���

		// ����IP��������ԭʼ�ֽ�����
		int ipPacketLen = sizeof(IPHeader) + testLen;
		U8* ipPacket = (U8*)malloc(ipPacketLen);
		if (!ipPacket) {
			free(testData);
			free(bitData);
			cout << "�ڴ治��" << endl;
			break;
		}
		IPHeader* ipHdr = (IPHeader*)ipPacket;
		ipHdr->srcIP = local_addr.sin_addr;
		ipHdr->destIP = lower_addr[0].sin_addr;
		memcpy(ipHdr->data, testData, testLen);

		// ��ӡIP�����ֽ�����
		cout << "\nIP��װ������ݣ��ֽ�����:" << endl;
		print_data_byte(ipPacket, ipPacketLen, 1);

		// ��IP��ת��Ϊ������
		int ipBitLen = ipPacketLen * 8;
		U8* ipBitData = (U8*)malloc(ipBitLen);
		if (!ipBitData) {
			free(testData);
			free(bitData);
			free(ipPacket);
			cout << "�ڴ治��" << endl;
			break;
		}
		int ipBitDataLen = ByteArrayToBitArray(ipBitData, ipBitLen, ipPacket, ipPacketLen);
		if (ipBitDataLen <= 0) {
			free(testData);
			free(bitData);
			free(ipPacket);
			free(ipBitData);
			cout << "IP���ֽڵ�����ת��ʧ��" << endl;
			break;
		}

		// ��ӡIP������������
		cout << "\nIP��װ������ݣ���������:" << endl;
		print_data_bit(ipBitData , ipBitDataLen , 0);

		// ������ת������
		cout << "\n����ת�����ԣ���������:" << endl;
		for (int i = 0; i < lowerNumber; i++) {
			cout << "��ӿ� " << i << " ת��������:" << endl;
			int sendRet = SendtoLower(ipBitData, ipBitDataLen, i);
			if (sendRet > 0) {
				iSndTotal += sendRet;
				iSndTotalCount++;
			}
			else {
				iSndErrorCount++;
			}
		}

		cout << "\n����ת�����ԣ���������:" << endl;
		int sendRet = SendtoUpper(ipBitData, ipBitDataLen);
		if (sendRet > 0) {
			iRcvToUpper += sendRet;
			iRcvToUpperCount++;
		}

		// �ͷ��ڴ�
		free(testData);
		free(bitData);
		free(ipPacket);
		free(ipBitData);
		break;
	}

	}
}