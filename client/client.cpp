#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <cstdlib>
#include <errno.h>
#include <utime.h>

using namespace std;

namespace fs = filesystem;

//#define PORT 1234

bool path_exists(const string& name) {
  struct stat buffer;   
  return (stat(name.c_str(), &buffer) == 0); 
}

bool dir_exists(const string& path) {
    struct stat buffer;

    if(stat(path.c_str(), &buffer) != 0)
        return 0;
    else if(buffer.st_mode & S_IFDIR)
        return 1;
    else
        return 0;
}

void convert_data(char* data, char* path, int& time, int& size) {
    
    string temp = data;

    int pos_f = temp.find("//");

    strcpy(path, temp.substr(0, pos_f - 1).c_str());
    
    int pos_s = temp.find("//", pos_f + 1);

    time = atoi(temp.substr(pos_f + 2, pos_s - pos_f - 2).c_str());

    pos_f = pos_s + 3;

    pos_s = temp.find("//", pos_f + 1);

    size = atoi(temp.substr(pos_f + 2, pos_s - pos_f - 2).c_str());
}

int readConfigFile(const char* file_path, string& address, int& port) {
    ifstream config_file(file_path);

    string folder_path {};
    string port_number {};

    getline(config_file, folder_path);

    fs::current_path(folder_path);

    getline(config_file, address);

    getline(config_file, port_number);

    port = stoi(port_number);

    return 1;
}

int main(int argc, char **argv) {

    if(argc != 2) {
        cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 0;
    }

    if(!path_exists(argv[1])) {
        cerr << "Path: " << argv[1] << " does not exist" << endl;
        return 0;
    }

    int port = 0;
    string ip {};

    readConfigFile(argv[1], ip, port);

    string path = fs::current_path().generic_string();

    int length_of_path = path.size() + 1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr_s{AF_INET, htons(port), 0, 0};
    inet_aton(ip.c_str(), &addr_s.sin_addr);

    connect(sock, (sockaddr*)&addr_s, sizeof(addr_s));

    while(1) {
    for (const fs::directory_entry& dir_entry : fs::recursive_directory_iterator(fs::current_path())){   

        if(fs::is_regular_file(dir_entry.path())) {
            write(sock, "</SYNC/>", 8);

            char buf[512] {};

            string dir_path = dir_entry.path().generic_string().erase(0, length_of_path);

            char buf1[512] {};
            int file_size = (int)fs::file_size(dir_entry);
                    
            struct stat result;
            stat(dir_entry.path().c_str(), &result);
                    
            int timestamp = result.st_mtime;

            snprintf(buf1, 512, "%s //%d// //%d//", dir_path.c_str(), timestamp, file_size);

            write(sock, buf1, 512);

            int rcv = read(sock, buf, 512);

            if(buf[2] == '0') {
                int src_file = open(dir_path.c_str(), O_RDONLY);

                        
                while(file_size > 0) {
                        
                    rcv = read(src_file, buf, 512);
                            
                    rcv = write(sock, buf, rcv);

                    file_size -= rcv;
                }
                close(src_file);

                rcv = read(sock, buf, 512);
            }
            
        }
    }


    write(sock, "</SEND/>", 8);

    char buf[512] {};

    read(sock, buf, 512);

    int num = (int)buf[2] - 48;

    char file_path[512] {};
    int timestamp = 0;
    int data_size = 0;
    int rcv = 0;
    while(num) {
        read(sock, buf, 512);

        convert_data(buf, file_path, timestamp, data_size);

        if(path_exists(file_path)) {
            struct stat result;
            stat(file_path, &result);
                
            int local_timestamp = result.st_mtime;

            if(timestamp > local_timestamp) {

                write(sock, "</0/>", 5);

                int dest_file = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

                while(data_size > 0) {
                    char file_buf[512] {};
                    rcv = read(sock, file_buf, 512);
                    rcv = write(dest_file, file_buf, rcv);
                    data_size -= rcv;
                }

                close(dest_file);

                struct utimbuf new_times;
                new_times.modtime = timestamp;
                utime(file_path, &new_times);
            }

            write(sock, "</1/>", 5);
        } else {
                
                

            if(int slashPos = string(file_path).find('/')) {
                    
                while(slashPos != -1) {
                    string folder_name = string(file_path).erase(slashPos, 512);
                        
                    if(dir_exists(folder_name) != 1) {
                        mkdir(folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                    }

                    string temp = string(file_path).erase(0, slashPos+1);
                    if(temp.find('/')+1 == 0) {
                        break;
                    }
                    slashPos += temp.find('/')+1;
                }
            }

            int dest_file = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

            write(sock, "</0/>", 5);

            while(data_size > 0) {
                char file_buf[512] {};
                rcv = read(sock, file_buf, 512);
                rcv = write(dest_file, file_buf, rcv);
                data_size -= rcv;
            }
            close(dest_file);
            struct utimbuf new_times;
            new_times.modtime = timestamp;
            utime(file_path, &new_times);
            write(sock, "</1/>", 5);
        }

        num--;
    }
    sleep(1);
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);

    return 0;
}