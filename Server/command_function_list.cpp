#include <iostream>
#include <string>
#include <vector>
#include <typeinfo>

using namespace std;
#define THIS_IS_SERVER

#include "server.h"

void* p_packet = &sendDataPacket;
void* recv_buf;
char* CID = new char[CID_size];
char* Hash = new char[Hash_size];
char* Signed_Hash = new char[Signed_Hash_size];
FILE* file;

void reshape_buffer(int type, int datasize){
	switch(type){
		case 0xa0 : case 0xc0 : 
					recv_buf = (char *)recv_buf;
					recv_buf = new char[datasize];
					break;
		case 0xa1 : recv_buf = (unsigned char*)recv_buf;
					recv_buf = new unsigned char[datasize];
					break;
		case 0xb0 : recv_buf = (int *)recv_buf;
					recv_buf = new int[datasize];
					break;
		case 0xb1 : recv_buf = (unsigned int*)recv_buf;
					recv_buf = new unsigned int[datasize];
					break;
	}
}

/*------------------public key send & response----------------------------*/
int public_key_send(HEADERPACKET* msg){

	FILE *file = fopen("PUBKEY.txt", "wb");
	reshape_buffer(msg->dataType, msg->dataSize);
	
	if(recv_binary(&g_pNetwork->port, msg->dataSize, recv_buf) == 0){
		cout << "recv_binary fail" << endl;
		return -1;
	}
	string sorder = "select key_ID from public_key where key_status = 1;";
	char* order = new char[sorder.length() + 1];
	strcpy(order, sorder.c_str());
	string key_ID = get_latest_key_ID(order);
	
	sorder = "update public_key set key_status = 0 where key_ID = '" + key_ID + "';";
	delete [] order;
	order = new char[sorder.length() + 1];
	strcpy(order, sorder.c_str());
	update_database(order);
	

	string pk((char*)recv_buf);
	char *key_value = new char[pk.length() + 1];
	strcpy(key_value, pk.c_str());
	insert_pk_database(getCID(), key_value);
	
	fwrite(recv_buf, sizeof(char), msg->dataSize, file);
	fflush(file);
	fclose(file);

	return 1;

}
int public_key_response(HEADERPACKET* msg){
	makePacket(Logger, PUBKEY_RES, 0xa0, 0);
	
	return send_binary(&g_pNetwork->port, CMD_HDR_SIZE, p_packet);
}
/*------------------------------------------------------------------------*/

/*-------------------video data send & response---------------------------*/
int video_data_send(HEADERPACKET* msg){
	reshape_buffer(msg->dataType, msg->dataSize);

	memset(recv_buf, 0, msg->dataSize);
	memset(CID, 0, CID_size);
	memset(Hash, 0, Hash_size);
	memset(Signed_Hash, 0, Signed_Hash_size);

	int frame_size =  msg->dataSize - CID_size - Hash_size - Signed_Hash_size;
	FILE *file;

	recv_binary(&g_pNetwork->port, CID_size, (void*)recv_buf);
	strcpy(CID, (char*)recv_buf);
	
	
	string s_dir = storage_dir;
	
	if(x != CID[9]){
		table_name = get_table_name();
		mkdir_func((s_dir + table_name).c_str());
		create_table();
		x = CID[9];
	}

	string frame_dir((const char*)recv_buf);
	frame_dir = s_dir + table_name + "/" + frame_dir; 
	const char* file_name = frame_dir.c_str();

	cout << "video file name: " << file_name << endl;

	file = fopen(file_name, "wb");

	if (file == NULL)
		cout << "file creation failed " << endl;
	memset(recv_buf, 0, msg->dataSize);

	recv_binary(&g_pNetwork->port, Hash_size, (void*)recv_buf);
	strcpy(Hash, (char*)recv_buf);
	memset(recv_buf, 0, msg->dataSize);

	recv_binary(&g_pNetwork->port, Signed_Hash_size, (void*)recv_buf);
	strcpy(Signed_Hash, (char*)recv_buf);
	memset(recv_buf, 0, msg->dataSize);

	recv_binary(&g_pNetwork->port, frame_size, (void*)recv_buf);
	fwrite(recv_buf, sizeof(char), frame_size, file);

	
	//cout << "CID: " << endl << CID << endl;
	//cout << "CID size:" << strlen(CID) << endl;

	//cout << "Hash: " << endl << Hash << endl;
	//cout << "Hash size: " << strlen(Hash) << endl;

	//cout << "Signed Hash: " << endl << Signed_Hash << endl;
	//cout << "Signed Hash: " << strlen(Signed_Hash) << endl;
	

	makePacket(Logger, VIDEO_DATA_RES, 0, 0);
	insert_database(CID, Hash, Signed_Hash);

	fflush(file);
	fclose(file);

 	send_binary(&g_pNetwork->port, sizeof(HEADERPACKET), p_packet);
	
	return 1;
}


int video_data_response(HEADERPACKET* msg){
	cout << "video data response recv" << endl;
	return 1;
}
/*------------------------------------------------------------------------*/

/*-----------------------Verify request & response------------------------*/
int verify_request(HEADERPACKET* msg){
	unsigned char* cid1 = new unsigned char[msg->dataSize];
	unsigned char* cid2 = new unsigned char[msg->dataSize];	

	recv_binary(&g_pNetwork->port, msg->dataSize, (void*)cid1);
	recv_binary(&g_pNetwork->port, msg->dataSize, (void*)cid2);

	string first_cid((char*)cid1);
	string last_cid((char*)cid2);

	cout << "first_cid : " << first_cid << endl;
	cout << "last_cid : " << last_cid << endl;

	if(first_cid > last_cid){
		first_cid.swap(last_cid);
	}

	cout << "first_cid : " << first_cid << endl;
	cout << "last_cid : " << last_cid << endl;
	vector<string> CID_list;
	vector<string> pk_list;

	CIDINFO start_cid = 
	{
		first_cid.substr(0, 4),
		first_cid.substr(5, 2),
		first_cid.substr(8, 2),
		first_cid.substr(11, 2),
		first_cid.substr(14, 2),
		first_cid.substr(17, 2),
		first_cid.substr(20,3),
	};

	CIDINFO end_cid = 
	{
		last_cid.substr(0, 4),
		last_cid.substr(5, 2),
		last_cid.substr(8, 2),
		last_cid.substr(11, 2),
		last_cid.substr(14, 2),
		last_cid.substr(17, 2),
		last_cid.substr(20,3),
	};

	if(start_cid.Day != end_cid.Day){
		vector<string> table_list;
		int j = stoi(end_cid.Day) - stoi(start_cid.Day);
		for(int i = 0; i <= j; i++){
			int d = i + stoi(start_cid.Day);
			string x_table = start_cid.Year + "_" + start_cid.Month + to_string(d);
			table_list.push_back(x_table);
		}
	}
	else{
		string vtable_name = start_cid.Year + '_' + start_cid.Month + start_cid.Day;
		
		get_list(pk_list, "public_key", "-1", first_cid, -1);
		get_list(pk_list, "public_key", first_cid, last_cid, 1);

		get_list(CID_list, vtable_name, first_cid, pk_list[1], 0);
		for(int i = 1; i < pk_list.size()-1; i++){
			get_list(CID_list, vtable_name, pk_list[i], pk_list[i + 1], 0);
		}
		get_list(CID_list, vtable_name, pk_list[pk_list.size()-1], last_cid, 0);
	}

// 	string s_time = start_cid.Year + "-" + start_cid.Month + "-" + start_cid.Day + "_" + start_cid.Hour + ":" + start_cid.Min + ":" + start_cid.Sec + ".500";
// 	string e_time = end_cid.Year + "-" + end_cid.Month + "-" + end_cid.Day + "_" + end_cid.Hour + ":" + end_cid.Min + ":" + end_cid.Sec + ".500";
// 	string t_name = start_cid.Year + "_" + start_cid.Month + start_cid.Day;
// 	string sorder = "select CID, Hash from " + t_name + " where CID between '" + s_time + "' and '" + e_time + "' order by CID;";

// 	cout << s_time << endl;
// 	cout << e_time << endl;
// 	cout << t_name << endl;
// 	cout << sorder << endl;
// 	char *order = new char[sorder.length() + 1];
// 	strcpy(order, sorder.c_str());
// 	res = mysql_perform_query(order);

// 	vector<string> cid_list;
// 	vector<string> hash_list;

// 	while((row = mysql_fetch_row(res)) != NULL){
// 		cid_list.push_back(row[0]);
// 		hash_list.push_back(row[1]);
// 	}

// 	unsigned char** frame_list = new unsigned char* [cid_list.size()];

// 	string frame_dir = "/home/pi/images/" + table_name + "/";
// 	size_t n;
// 	int c;
// 	for(int i=0; i < cid_list.size(); i++){
// 		n = 0;
// 		string frame_name = frame_dir + cid_list[i];
// 		const char* frame = frame_name.c_str();
// 		FILE* file = fopen(frame, "rb");

// 		fseek(file, 0, SEEK_END);
// 		int size = ftell(file);
// 		frame_list[i] = new unsigned char[size];
// 		fseek(file,0,0);

// 		while((c = fgetc(file)) != EOF){
// 			frame_list[i][n++] = (unsigned char)c;
// 		}
// 	}
	// makePacket(Verifier, VIDEO_DATA_SND, 0xa1, CID_size + Hash_size + strlen((char*)frame_list[0]));

	// for(int i=0; i<cid_list.size(); i++){
	// 	unsigned char* cid = new unsigned char[CID_size];
	// 	unsigned char* hash = new unsigned char[Hash_size];
	// 	unsigned char* buf = new unsigned char[CMD_HDR_SIZE];
	// 	strcpy((char*)cid, cid_list[i].c_str());
	// 	strcpy((char*)hash, hash_list[i].c_str());
	// 	int res;

	// 	void* p_packet = &sendDataPacket;

	// 	if(!send_binary(&g_pNetwork->port, sizeof(HEADERPACKET), p_packet)){
    //         cout << "Packet send Error!!" << endl;
    //         break;
    //     }
	// 	if(!send_binary(&g_pNetwork->port, CID_size, (void*)cid)){
    //         cout << "CID send Error!!" << endl;
    //     }

    //     if(!send_binary(&g_pNetwork->port, Hash_size, (void*)hash)){
    //         cout << "hash send Error!!" << endl;
    //     }

    //     if(!send_binary(&g_pNetwork->port, strlen((char*)frame_list[0]), (void*)frame_list[i])){
    //         cout << "Image send Error!!" << endl;
    //     }

		// if(recv((int)&g_pNetwork->port.s, buf, CMD_HDR_SIZE, 0) > 0){
		// 	// if((HEADERPACKET*)buf->command == VIDEO_DATA_RES){
		// 	// 	continue;
		// 	// }
		// 	else{
		// 		return -1;
		// 	}
		// }
	//}
	// FILE* file1 = fopen("test", "wb");
	// fwrite(frame_list[23], sizeof(char), strlen((char*)frame_list[23]), file1);	
	// fflush(file1);
	// fclose(file1);
	
	// for(int i=0;i<cid_list.size();i++){
	// 	cout << cid_list[i] << endl;
	// }

	// for(int i=0;i<hash_list.size();i++){
	// 	cout << hash_list[i] << endl;
	// }

// 	return 1;
}

int verify_response(HEADERPACKET* msg){
	reshape_buffer(msg->dataType, msg->dataSize);
	recv_binary(&g_pNetwork->port, msg->dataSize, (void*)recv_buf);
}
/*------------------------------------------------------------------------*/

/*--------------------------Verify result send----------------------------*/
int verified_result_send(HEADERPACKET* msg){

}
int verified_result_response(HEADERPACKET* msg){

}
/*------------------------------------------------------------------------*/


/*-------------------------Hash request & response------------------------*/
int hash_request(HEADERPACKET* msg){
	makePacket(Logger, HASH_REQ, 0, 0);
	send_binary(&g_pNetwork->port, sizeof(HEADERPACKET), p_packet);

	return 1;
}

int hash_send(HEADERPACKET* msg){
	cout << "hash request receive";
	return 1;
}
/*------------------------------------------------------------------------*/



/*
 This function is for test. Receive data and write down .txt file. 
 commmad : 0xff
 dataType : 0xa0 = char
			0xa1 = unsigned char
			0xb0 = int
			0xb1 = unsigned int
*/
int test(HEADERPACKET* msg){
	FILE *file = fopen("test.txt", "wb");
	reshape_buffer(msg->dataType, msg->dataSize);
	
	if(recv_binary(&g_pNetwork->port, msg->dataSize, recv_buf) == 0){
		cout << "recv_binary fail" << endl;
		return -1;
	}
	
	fwrite(recv_buf, sizeof(char), msg->dataSize, file);
	
	fflush(file);
	fclose(file);

	return 1;
}