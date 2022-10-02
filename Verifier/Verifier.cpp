#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h> 
#include <fstream>
#include <string>
#include <errno.h>

#include "Verifier_function_list.h"
#include "verify.cpp"
#include "../Merkle_Tree/merkle_tree.h"
#include "verifier_cfg.h"
#include "../msg_queue/msg_queue.cpp"
#include "../DB/bout_database.cpp"

using namespace std;
using namespace cv;

queue<cv::Mat> yuv420_queue;                    //for original frame(yuv)Mat queue
queue<cv::Mat> y_queue;                         //for y_frame Mat queue
queue<cv::Mat> feature_vector_queue;            //for edge detection Canny
queue<string> hash_DB_queue;                    //for hash from server
queue<string> hash_verifier_queue;              //for hash made by feature vector by verifier
queue<string> cid_queue;                        //for CID for images 

int read_pubKey(){
    ifstream file(pubKey_path);

    if(file.is_open()){
        while(getline(file, publicKey)){
            cout << publicKey << endl;
        }
    } else{
        cout << "Unable to get Pubkey" << endl;
    }
    file.close();
    return 0;
}

void get_data_from_DB(string &CID, queue<string> &CID_QUEUE, queue<string> &HASH_DB_QUEUE){
	init_DB();
	string table_name = CID.substr(0,4) + "_" + CID.substr(5,2) + CID.substr(8,2);
    string sorder = "select CID, Hash from " + table_name + " where Verified = 0 order by CID DESC limit 10; ";

    char *order = new char[sorder.length() + 1];
    strcpy(order, sorder.c_str());

    cout << order << endl;
	res = mysql_perform_query(conn, order);

	while((row = mysql_fetch_row(res)) != NULL){
		CID_QUEUE.push(row[0]);
		HASH_DB_QUEUE.push(row[1]);
        cout << row[1];
	}

    cout << "CID queue size: " << CID_QUEUE.size() << endl;
    cout << "HASH queue size: " << HASH_DB_QUEUE.size() << endl;
}

void read_video_data(string &CID , queue<string> &CID_QUEUE, queue<cv::Mat> &YUV420_QUEUE) {
    queue<string> get_CID(CID_QUEUE);

    string folder_dir = CID.substr(0,4) + "_" + CID.substr(5,2) + CID.substr(8,2) + "/";
    string file_dir = video_data_path + folder_dir;
    cout << file_dir << endl;

    while(true) {
        if(get_CID.size() == 0) {break;}
        string frame_name = file_dir + get_CID.front();
        const char* frame = frame_name.c_str();
        cout << frame;

        FILE *frame_file = fopen(frame, "r+");
        fseek(frame_file, 0, SEEK_END);
		int size = ftell(frame_file);

        fseek(frame_file, 0, 0);
        
        unsigned char *frame_data = (unsigned char*) malloc(size);
        fread(frame_data, sizeof(unsigned char), size, frame_file);

        fflush(frame_file);
        fclose(frame_file);

        if(size == VGA_SIZE){
            cv::Mat frame(Size(YUV420_VGA_WIDTH, YUV420_VGA_HEIGHT), CV_8UC1, *frame_data);
            cout << frame.size() << endl;
            YUV420_QUEUE.push(frame);
        }
        else if(size == CIF_SIZE){
            cv::Mat frame(Size(YUV420_CIF_WIDTH, YUV420_CIF_HEIGHT), CV_8UC1, *frame_data);
            cout << frame.size() << endl;
            YUV420_QUEUE.push(frame);
        }

        get_CID.pop();
    }
    cout << "Frame read " << YUV420_QUEUE.size() << endl;
}

void convert_frames(queue<cv::Mat> &YUV420_QUEUE){
    cout << endl << "----Start to convert Frames into Y----" << endl << endl;
    queue<cv::Mat> yuv_queue(YUV420_QUEUE);

    while(true){
        if(yuv_queue.size() == 0) {break;}
        cv::Mat yuv_frame = yuv_queue.front();
        int height = yuv_frame.rows;
        int width = yuv_frame.cols;

        cv::Mat y_frame(cv::Size(height, width), CV_8UC1);
        yuv_queue.pop();

        //CONVERT BGR To YUV420 and YUV420 to Y
        cv::cvtColor(yuv_frame, y_frame, cv::COLOR_YUV2GRAY_I420);
    
        //save frames into queue 
        y_queue.push(y_frame);
        
    }
    
    cout << "    Y frame are saved" << endl;
    cout << "    y_queue size: "  << y_queue.size() << endl << endl;
    cout << "----FRAMES CONVERTED---------" << endl << endl;
}

void edge_detection(queue<cv::Mat> &Y_QUEUE) {
    queue<cv::Mat> Y_queue(Y_QUEUE);
    
    cout << "----Building feature vectors." << endl;
    int cnt = 0;
    
    while(true) {
        if(Y_queue.size() == 0) {break;}
        cv::Mat temp;
        
        //Canny(img, threshold1, threshold2) 
        //threshold1 = Determining whether an edge is in the adjacency with another edge
        //threshold2 = Determine if it is an edge or not
        cv::Canny(Y_queue.front(), temp, 20, 40);
        
        feature_vector_queue.push(temp);
        Y_queue.pop();
        cnt++;
    }
    cout << endl << "    Edge Detection made: " << feature_vector_queue.size() << endl;
}

void make_hash(queue<cv::Mat> &FV_QUEUE) {
    
    queue<cv::Mat> Feature_Vector_queue(FV_QUEUE);
    cout << endl << "----Make HASH from feature vectors." << endl << endl;
    
    while(true) {
        if(Feature_Vector_queue.size() == 0) {break;}
        cv::Mat temp = Feature_Vector_queue.front();
        Feature_Vector_queue.pop();
        
        string mat_data = "";
        string sha_result = "";
        
        
        for(int i =0; i<temp.rows; i++) {
            for(int j =0; j < temp.cols; j++) {
                mat_data += to_string(temp.at<uchar>(i,j));
            }
        }
        
        
        sha_result = hash_sha256(mat_data);
        hash_verifier_queue.push(sha_result);
    }
    cout << "    hash made : " << hash_verifier_queue.size() << endl;
}


int make_merkle_tree(queue<string> &HASH_DB_QUEUE, queue<string> &HASH_VERIFIER_QUEUE){
    vector<Node*> leaves_DB;
    vector<Node*> leaves_Verifier;
    /*
    queue<string> from_DB(HASH_DB_QUEUE);
    queue<string> from_VERIFIER(HASH_VERIFIER_QUEUE);

    int i = 0;
    while(true) {
        if(from_DB.size() == 0 && from_VERIFIER.size() == 0) {break;}
        string hash_DB = from_DB.front();
        string hash_Verifier = from_VERIFIER.front();

        leaves_DB.push_back(new Node(hash_DB));
        leaves_Verifier.push_back(new Node(hash_Verifier));
        i = i+1;
    }
    
    // initialize leaves
    for (unsigned int i = 0; i < leaves.size(); i++) {
        leaves_DB[i]-> left = NULL;
        leaves_DB[i]-> right = NULL;
        leaves_Verifier[i]-> left = NULL;
        leaves_Verifier[i]-> right = NULL;
    }

    MerkleTree *hashTreeDB = new MerkleTree(leaves_DB);
    MerkleTree *hashTreeVerifier = new MerkleTree(leaves_Verifier);

    std::cout << hashTreeDB->root->hash << std::endl;
    std::cout << hashTreeVerifier->root->hash << std::endl;

    hashTreeDB->printTree(hashTreeDB->root, 0);
    hashTreeVerifier->printTree(hashTreeVerifier->root, 0);

    for (unsigned int k = 0; k < leaves_DB.size(); k++) {
        delete leaves_DB[k];
        delete leaves_Verifier[k];
    }
    delete hashTreeDB;
    delete hashTreeVerifier;
    return 0;
    */
}

/*
void init_all_setting() {
    init_queue();
    
}

void init_queue() {         
    yuv420_queue = queue<cv::Mat>();                 //for original frame(yuv)Mat queue
    y_queue = queue<cv::Mat>();                      //for y_frame Mat queue
    feature_vector_queue = queue<cv::Mat>();         //for edge detection Canny
    hash_DB_queue = queue<string>();
    hash_verifier_queue = queue<string>();          
    cid_queue = queue<string>();                     //for CID for frames
}
*/
/*

*/

int main() { 
    read_pubKey();
    
    string S_CID = "2022-10-02_10:30:10.111\0";
    string V_CID = "";
    Server2Verifier_CID_send(S_CID);
    Server2Verifier_CID_recv(V_CID);

    Verifier2Server_CID_recv_send();
    Verifier2Server_CID_recv_recv();
    
    get_data_from_DB(V_CID, cid_queue, hash_DB_queue);
    read_video_data(V_CID, cid_queue,yuv420_queue);

    convert_frames(yuv420_queue);
    edge_detection(y_queue);
    make_hash(feature_vector_queue);
    
    make_merkle_tree(hash_DB_queue, hash_verifier_queue);

    return 0;
}
 
