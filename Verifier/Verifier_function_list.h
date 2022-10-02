#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <string>

using namespace std;

int read_pubKey();
void init();
void init_queue();
void get_data_from_DB(string &CID, queue<string> &CID_QUEUE, queue<string> &HASH_DB_QUEUE);	          //Get Datas from SERVER
void read_video_data(string &CID , queue<string> &CID_QUEUE, queue<cv::Mat> &YUV420_QUEUE);
void convert_frames(queue<cv::Mat> &YUV420_QUEUE);	  //Convert Datas
void edge_detection(queue<cv::Mat> &Y_QUEUE);         //Edge detact by y frames
void make_hash(queue<cv::Mat> &FV_QUEUE);             //make hash using feature vector
int make_merkle_tree(queue<string> &HASH_DB_QUEUE, queue<string> &HASH_VERIFIER_QUEUE);
string getCID();                                      //Make CID for each frames
int coompare_hashs();								  //compare two hashs                                  //Make CID for frames

