//Nettester 的功能文件
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
using namespace std;

//以下为重要的变量
U8* sendbuf;        //用来组织发送数据的缓存，大小为MAX_BUFFER_SIZE,可以在这个基础上扩充设计，形成适合的结构，例程中没有使用，只是提醒一下
int printCount = 0; //打印控制
int spin = 0;  //打印动态信息控制

//------一些统计用的全局变量------------
int iSndTotal = 0;  //发送数据总量
int iSndTotalCount = 0; //发送数据总次数
int iSndErrorCount = 0;  //发送错误次数
int iRcvForward = 0;     //转发数据总量
int iRcvForwardCount = 0; //转发数据总次数
int iRcvToUpper = 0;      //从低层递交高层数据总量
int iRcvToUpperCount = 0;  //从低层递交高层数据总次数
int iRcvUnknownCount = 0;  //收到不明来源数据总次数



//打印统计信息
void print_statistics();
void menu();
//***************重要函数提醒******************************
//名称：InitFunction
//功能：初始化功能面，由main函数在读完配置文件，正式进入驱动机制前调用
//输入：
//输出：
void InitFunction(CCfgFileParms& cfgParms)
{
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL ) {
		cout << "内存不够" << endl;
		exit(0);
	}
	return;
}
//***************重要函数提醒******************************
//名称：EndFunction
//功能：结束功能面，由main函数在收到exit命令，整个程序退出前调用
//输入：
//输出：
void EndFunction()
{
	if(sendbuf != NULL)
		free(sendbuf);
	return;
}

//-----------------------------------------------------------------------------------
// 以下内容是ip数据包

// 路由表项结构
struct RouteEntry {
	in_addr destIP;      // 目的IP地址
	in_addr nextHop;     // 下一跳IP
	int outPort;         // 出接口号
	bool isValid;        // 是否有效
};

// 全局变量
const int MAX_ROUTE_ENTRIES = 20;  // 最大路由表项数
RouteEntry routeTable[MAX_ROUTE_ENTRIES];  // 路由表
int routeCount = 0;  // 当前路由表项数
// IP-Port映射表项结构 (类似ARP表)
// IP-Port映射表项结构 (类似ARP表)
struct IPPortMapping {
	in_addr ipAddr;      // IP地址
	int port;           // 对应的端口号 
	bool isValid;       // 是否有效

	// MAC地址结构(6字节)
	struct MacAddress {
		U8 bytes[6];    // MAC地址字节数组
	} mac;

	// 根据IP地址生成MAC地址
	void generateMACFromIP() {
		// 清零MAC地址
		memset(mac.bytes, 0, sizeof(mac.bytes));

		// 从IP地址(127.0.x.x格式)提取网元号和端口号
		UCHAR* ipBytes = (UCHAR*)&ipAddr.S_un.S_addr;

		// 网元号在第3个字节(x.x.DeviceID.x)
		int devNum = ipBytes[2];  // 获取网元号
		mac.bytes[0] = 0;         // 高字节设为0
		mac.bytes[1] = devNum;    // 低字节为网元号

		// 端口号在第4个字节(x.x.x.PortID)
		int portNum = ipBytes[3]; // 获取端口号
		mac.bytes[2] = 0;        // 高字节设为0 
		mac.bytes[3] = portNum;  // 低字节为端口号

		// 最后两个字节在错误的时候把地端口号进行替换
		mac.bytes[4] = 1;
		mac.bytes[5] = 2;
	}

	// 获取MAC地址的字符串表示
	string getMACString() const {
		char macStr[18];
		snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
			mac.bytes[0], mac.bytes[1], mac.bytes[2],
			mac.bytes[3], mac.bytes[4], mac.bytes[5]);
		return string(macStr);
	}
};

// 在function.cpp开头添加全局变量
const int MAX_IP_PORT_MAPPINGS = 20;
IPPortMapping ipPortMappings[MAX_IP_PORT_MAPPINGS];
int mappingCount = 0;

// 添加lookupIPPort函数的实现
int lookupIPPort(in_addr destIP) {
	for (int i = 0; i < mappingCount; i++) {
		if (ipPortMappings[i].isValid &&
			ipPortMappings[i].ipAddr.S_un.S_addr == destIP.S_un.S_addr) {
			return ipPortMappings[i].port;
		}
	}
	return -1;
}
// IP头部结构
struct IPHeader {
	in_addr srcIP;   // 源IP
	in_addr destIP;  // 目的IP
	U8 data[0];      // 数据部分
};



//---------------------------------------------------------------------------------

//***************重要函数提醒******************************
//名称：TimeOut
//功能：本函数被调用时，意味着sBasicTimer中设置的超时时间到了，
//      函数内容可以全部替换为设计者自己的想法
//      例程中实现了几个同时进行功能，供参考
//      1)根据iWorkMode工作模式，判断是否将键盘输入的数据发送，
//        因为scanf会阻塞，导致计时器在等待键盘的时候完全失效，所以使用_kbhit()无阻塞、不间断地在计时的控制下判断键盘状态，这个点Get到没？
//      2)不断刷新打印各种统计值，通过打印控制符的控制，可以始终保持在同一行打印，Get？
//输入：时间到了就触发，只能通过全局变量供给输入
//输出：这就是个不断努力干活的老实孩子
void TimeOut()
{

	printCount++;
	if (_kbhit()) {
		//键盘有动作，进入菜单模式
		menu();
	}

	print_statistics();
}
//------------华丽的分割线，以下是数据的收发,--------------------------------------------

//***************重要函数提醒******************************
//名称：RecvfromUpper
//功能：本函数被调用时，意味着收到一份高层下发的数据
//      函数内容全部可以替换成设计者自己的
//      例程功能介绍
//         1)通过低层的数据格式参数lowerMode，判断要不要将数据转换成bit流数组发送，发送只发给低层接口0，
//           因为没有任何可供参考的策略，讲道理是应该根据目的地址在多个接口中选择转发的。
//         2)判断iWorkMode，看看是不是需要将发送的数据内容都打印，调试时可以，正式运行时不建议将内容全部打印。
//输入：U8 * buf,高层传进来的数据， int len，数据长度，单位字节
//输出：
void RecvfromUpper(U8* buf, int len) {
	int iSndRetval = 0;
	U8* bufSend = NULL;

	in_addr destIP;
	memcpy(&destIP, buf, sizeof(in_addr));

	int ipPacketLen = sizeof(IPHeader) + len - sizeof(in_addr);
	U8* ipPacket = (U8*)malloc(ipPacketLen);
	if (!ipPacket) {
		cout << "内存不足" << endl;
		return;
	}

	IPHeader* ipHdr = (IPHeader*)ipPacket;
	ipHdr->srcIP = local_addr.sin_addr;
	ipHdr->destIP = destIP;
	memcpy(ipHdr->data, buf + sizeof(in_addr), len - sizeof(in_addr));

	int outPort = lookupIPPort(destIP);
	if (outPort < 0) {
		printf("\n[NET]未找到IP %s的端口映射,广播转发\n", inet_ntoa(destIP));
		for (int i = 0; i < lowerNumber; i++) {
			if (lowerMode[i] == 0) {
				bufSend = (U8*)malloc(ipPacketLen * 8);
				if (!bufSend) {
					free(ipPacket); // 释放 ipPacket
					cout << "内存不足" << endl;
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
		printf("\n[NET]转发IP包 %s -> %s 从端口 %d\n",
			inet_ntoa(ipHdr->srcIP), inet_ntoa(ipHdr->destIP), outPort);
		if (lowerMode[outPort] == 0) {
			bufSend = (U8*)malloc(ipPacketLen * 8);
			if (!bufSend) {
				free(ipPacket); // 释放 ipPacket
				cout << "内存不足" << endl;
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

	free(ipPacket); // 确保释放

	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iSndTotal += iSndRetval;
		iSndTotalCount++;
	}
}

//***************重要函数提醒******************************
//名称：RecvfromLower
//功能：本函数被调用时，意味着得到一份从低层实体递交上来的数据
//输入：U8 * buf,低层递交上来的数据， int len，数据长度，单位字节，int ifNo ，低层实体号码，用来区分是哪个低层
//输出：
void RecvfromLower(U8* buf, int len, int ifNo) {
	int iSndRetval = 0; // 初始化
	U8* bufSend = NULL;

	if (len < sizeof(IPHeader)) {
		cout << "接收到的数据太短,无法解析IP头" << endl;
		return;
	}

	IPHeader* ipHdr = (IPHeader*)buf;

	if (ipHdr->destIP.S_un.S_addr == local_addr.sin_addr.S_un.S_addr) {
		if (lowerMode[ifNo] == 0) {
			int byteLen = (len + 7) / 8;
			bufSend = (U8*)malloc(byteLen);
			if (bufSend == NULL) {
				cout << "内存不足" << endl;
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
					cout << "内存不足" << endl;
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
		cout << "共转发 "<< iRcvForward<< " 位，"<< iRcvForwardCount<<" 次，"<<"递交 "<< iRcvToUpper<<" 位，"<< iRcvToUpperCount<<" 次,"<<"发送 "<< iSndTotal <<" 位，"<< iSndTotalCount<<" 次，"<< "发送不成功 "<< iSndErrorCount<<" 次,""收到不明来源 "<< iRcvUnknownCount<<" 次。";
		spin++;
	}

}
//PrintParms 打印工作参数，注意不是cfgFilms读出来的，而是目前生效的参数
void PrintParms()
{
	size_t i;
	cout << "设备号: " << strDevID << " 层次: " << strLayer << "实体: " << strEntity << endl;
	cout << "上层实体地址: " << inet_ntoa(upper_addr.sin_addr) << "  UDP端口号: " << ntohs(upper_addr.sin_port) << endl;

	cout << "本层实体地址: " << inet_ntoa(local_addr.sin_addr) << "  UDP端口号: " << ntohs(local_addr.sin_port) << endl;
	if (strLayer.compare("PHY") == 0) {
		if (lowerNumber <= 1) {
			cout << "下层点到点信道" << endl;
			cout << "链路对端地址: ";
		}
		else {
			cout << "下层广播式信道" << endl;
			cout << "共享信道站点：";
		}
	}
	else {
		cout << "下层实体";
	}
	if (lowerNumber == 1) {
		cout << "地址：" << inet_ntoa(lower_addr[0].sin_addr) << "  UDP端口号: " << ntohs(lower_addr[0].sin_port) << endl;
	}
	else {
		if (strLayer.compare("PHY") == 0) {
			cout << endl;
			for (i = 0; i < lowerNumber; i++) {
				cout << "        地址：" << inet_ntoa(lower_addr[i].sin_addr) << "  UDP端口号: " << ntohs(lower_addr[i].sin_port) << endl;
			}
		}
		else {
			cout << endl;
			for (i = 0; i < lowerNumber; i++) {
				cout << "        接口: [" << i << "] 地址" << inet_ntoa(lower_addr[i].sin_addr) << "  UDP端口号: " << ntohs(lower_addr[i].sin_port) << endl;
			}
		}
	}
	string strTmp;
	//strTmp = getValueStr("cmdIpAddr");
	cout << "统一管理平台地址: " << inet_ntoa(cmd_addr.sin_addr);
	//strTmp = getValueStr("cmdPort");
	cout << "  UDP端口号: " << ntohs(cmd_addr.sin_port) << endl;
	//strTmp = getValueStr("oneTouchAddr");
	cout << "oneTouch一键启动地址: " << inet_ntoa(oneTouch_addr.sin_addr);
	//strTmp = getValueStr("oneTouchPort");
	cout << "  UDP端口号; " << ntohs(oneTouch_addr.sin_port) << endl;
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
	//发送|打印：[发送控制（0，等待键盘输入；1，自动）][打印控制（0，仅定期打印统计信息；1，按bit流打印数据，2按字节流打印数据]
	cout << endl << endl << "设备号:" << strDevID << ",    层次:" << strLayer << ",    实体号:" << strEntity ;
	printf("\n");
	cout << "8-自动数据封装和转发测试" << endl;
	cout << endl << "0-取消" << endl << "请输入数字选择命令：";
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
		cout << "自动数据封装和比特流转发测试:" << endl;

		// 构造测试数据
		const int testDataSize = 100; // 合理大小
		U8* testData = (U8*)malloc(testDataSize);
		if (!testData) {
			cout << "内存不足" << endl;
			break;
		}
		int testLen = snprintf((char*)testData, testDataSize, "Test Message");
		if (testLen < 0 || testLen >= testDataSize) {
			free(testData);
			cout << "测试数据构造失败" << endl;
			break;
		}

		// 打印原始数据（字节流）
		cout << "原始数据（字节流）:" << endl;
		print_data_byte(testData, testLen, 1);

		// 转换为比特流
		int bitLen = testLen * 8; // 每个字节8位
		U8* bitData = (U8*)malloc(bitLen);
		if (!bitData) {
			free(testData);
			cout << "内存不足" << endl;
			break;
		}
		int bitDataLen = ByteArrayToBitArray(bitData, bitLen, testData, testLen);
		if (bitDataLen <= 0) {
			free(testData);
			free(bitData);
			cout << "字节到比特转换失败" << endl;
			break;
		}

		// 打印比特流
		cout << "\n测试数据（比特流）:" << endl;
		print_data_bit(bitData , bitDataLen ,0); // 假设存在打印比特流的函数

		// 构造IP包（基于原始字节流）
		int ipPacketLen = sizeof(IPHeader) + testLen;
		U8* ipPacket = (U8*)malloc(ipPacketLen);
		if (!ipPacket) {
			free(testData);
			free(bitData);
			cout << "内存不足" << endl;
			break;
		}
		IPHeader* ipHdr = (IPHeader*)ipPacket;
		ipHdr->srcIP = local_addr.sin_addr;
		ipHdr->destIP = lower_addr[0].sin_addr;
		memcpy(ipHdr->data, testData, testLen);

		// 打印IP包（字节流）
		cout << "\nIP封装后的数据（字节流）:" << endl;
		print_data_byte(ipPacket, ipPacketLen, 1);

		// 将IP包转换为比特流
		int ipBitLen = ipPacketLen * 8;
		U8* ipBitData = (U8*)malloc(ipBitLen);
		if (!ipBitData) {
			free(testData);
			free(bitData);
			free(ipPacket);
			cout << "内存不足" << endl;
			break;
		}
		int ipBitDataLen = ByteArrayToBitArray(ipBitData, ipBitLen, ipPacket, ipPacketLen);
		if (ipBitDataLen <= 0) {
			free(testData);
			free(bitData);
			free(ipPacket);
			free(ipBitData);
			cout << "IP包字节到比特转换失败" << endl;
			break;
		}

		// 打印IP包（比特流）
		cout << "\nIP封装后的数据（比特流）:" << endl;
		print_data_bit(ipBitData , ipBitDataLen , 0);

		// 比特流转发测试
		cout << "\n向下转发测试（比特流）:" << endl;
		for (int i = 0; i < lowerNumber; i++) {
			cout << "向接口 " << i << " 转发比特流:" << endl;
			int sendRet = SendtoLower(ipBitData, ipBitDataLen, i);
			if (sendRet > 0) {
				iSndTotal += sendRet;
				iSndTotalCount++;
			}
			else {
				iSndErrorCount++;
			}
		}

		cout << "\n向上转发测试（比特流）:" << endl;
		int sendRet = SendtoUpper(ipBitData, ipBitDataLen);
		if (sendRet > 0) {
			iRcvToUpper += sendRet;
			iRcvToUpperCount++;
		}

		// 释放内存
		free(testData);
		free(bitData);
		free(ipPacket);
		free(ipBitData);
		break;
	}

	}
}