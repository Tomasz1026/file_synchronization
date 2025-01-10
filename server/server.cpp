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
#include <vector>
#include <utime.h>

using namespace std;

namespace fs = filesystem;

#define PORT 1234

vector<string> server_files {};

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

int updateServerFilesList() {
    
    server_files.clear();

    string path = fs::current_path().generic_string();

    int length_of_path = path.size() + 1;

    for (const fs::directory_entry& dir_entry : fs::recursive_directory_iterator(fs::current_path())){   

        if(fs::is_regular_file(dir_entry.path())) {
            string dir_path = dir_entry.path().generic_string().erase(0, length_of_path);

            char buf1[512] {};
            int file_size = (int)fs::file_size(dir_entry);
                    
            struct stat result;
            stat(dir_entry.path().c_str(), &result);
                    
            int timestamp = result.st_mtime;

            snprintf(buf1, 512, "%s //%d// //%d//", dir_path.c_str(), timestamp, file_size);

            server_files.push_back(buf1);
        }
    }
    
    return 1;
}

int receiveFromCli(int client) {
    char data[512] {};
    int rcv = read(client, data, 8);

    if(rcv<=0) {
        return 0;
    }

    if(strcmp(data, "</SYNC/>") == 0) {
        
        memset(data, 0, 512);

        rcv = read(client, data, 512);
        if(rcv<=0) {
            return 0;
        }

        char file_path[512] {};
        int timestamp = 0;
        int data_size = 0;

        convert_data(data, file_path, timestamp, data_size);

        if(path_exists(file_path)) {
            struct stat result;
            stat(file_path, &result);
                
            int local_timestamp = result.st_mtime;

            if(timestamp > local_timestamp) {

                write(client, "</0/>", 5);

                int dest_file = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

                while(data_size > 0) {
                    char file_buf[512] {};
                    rcv = read(client, file_buf, 512);
                    if(rcv<=0) {
                        return 0;
                    }

                    rcv = write(dest_file, file_buf, rcv);
                    data_size -= rcv;
                }

                close(dest_file);
                struct utimbuf new_times;
                new_times.modtime = timestamp;
                utime(file_path, &new_times);
            }

            write(client, "</1/>", 5);
            
        } else {
                
            int slashPos = string(file_path).find('/');
            if(slashPos != -1) {
                
                string temp {};
                string folder_name {};

                do {
                    folder_name = string(file_path).erase(slashPos, 512);
                        
                    if(dir_exists(folder_name) != 1) {
                        mkdir(folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                    }

                    temp = string(file_path).erase(0, slashPos+1);
                    
                    slashPos += temp.find('/')+1;
                } while(temp.find('/')+1 != 0);
            }
            

            int dest_file = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

            write(client, "</0/>", 5);

            while(data_size > 0) {
                char file_buf[512] {};
                rcv = read(client, file_buf, 512);
                if(rcv<=0) {
                    return 0;
                }

                rcv = write(dest_file, file_buf, rcv);
                data_size -= rcv;
            }
            close(dest_file);
            struct utimbuf new_times;
            new_times.modtime = timestamp;
            utime(file_path, &new_times);
            write(client, "</1/>", 5);
        }
    } else if(strcmp(data, "</SEND/>") == 0) {
        
        char buf1[512] {};

        snprintf(buf1, 512, "</%ld/>", server_files.size());

        write(client, buf1, 512);

        for(int i=0; i<(int)server_files.size(); i++) {
            write(client, server_files[i].c_str(), 512);
            
            rcv = read(client, buf1, 512);
            if(rcv<=0) {
                return 0;
            }

            if (strcmp(buf1, "</0/>") == 0) {
                char buf[512] {};
                strcpy(buf, server_files[i].c_str());
                char file_path[512] {};
                int timestamp = 0;
                int data_size = 0;

                convert_data(buf, file_path, timestamp, data_size);

                int dest_file = open(file_path, O_RDONLY);

                while(data_size > 0) {
                    char file_buf[512] {};
                    rcv = read(dest_file, file_buf, 512);
                    if(rcv<=0) {
                        return 0;
                    }

                    rcv = write(client, file_buf, rcv);
                    data_size -= rcv;
                }
                close(dest_file);
                
                rcv = read(client, buf1, 512);
                if(rcv<=0) {
                    return 0;
                }

            }
        }
    }

    

    return 1;
}

int readConfigFile(const char* file_path, int& port) {
    ifstream config_file(file_path);

    string folder_path {};
    string port_number {};

    getline(config_file, folder_path);

    fs::current_path(folder_path);

    getline(config_file, port_number);

    port = stoi(port_number);

    return 1;
}

int main(int argc, char** argv) {

    if(argc != 2) {
        cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 0;
    }

    if(!path_exists(argv[1])) {
        cerr << "Path: " << argv[1] << " does not exist" << endl;
        return 0;
    }

    int port = 0;
    readConfigFile(argv[1], port);
    updateServerFilesList();

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr{AF_INET, htons(port), INADDR_ANY, 0};
    bind(server_sock, (sockaddr*)&addr, sizeof(addr));

    listen(server_sock, SOMAXCONN);

    int epollDescr = epoll_create1(0);
    
    epoll_event ee[2];
    ee[0].events = EPOLLIN;
    ee[0].data.u32 = 1;
    epoll_ctl(epollDescr, EPOLL_CTL_ADD, server_sock, &ee[0]);

    for(;;) {

        int ready = epoll_wait(epollDescr, ee, 2, -1);

        for(int i = 0; i < ready; i++) {
            if(ee[i].data.u32 == 1) {
                int client = accept(server_sock, 0, 0);
                ee[0].events = EPOLLIN ;
                ee[0].data.u32 = client;
                epoll_ctl(epollDescr, EPOLL_CTL_ADD, client, &ee[0]);
                cout << "Client accepted" << endl;
            } else {
                if(ee[i].events & EPOLLIN) {
                    if(!receiveFromCli(ee[i].data.u32)){
                        epoll_ctl(epollDescr, EPOLL_CTL_DEL, ee[i].data.u32, &ee[0]);
                        shutdown(ee[i].data.u32, SHUT_RDWR);
                        close(ee[i].data.u32);
                        cout << "Client deleted" << endl;
                    }
                }
               
            }
        }
        updateServerFilesList();
    }

    shutdown(server_sock, SHUT_RDWR);
    close(server_sock);

    return 0;
}

    