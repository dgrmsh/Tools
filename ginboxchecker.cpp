#include </usr/local/include/curl/curl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <string.h>
#include <thread>
#include <time.h>
#include <unistd.h>

CURLcode parseConfig(std::map<std::string, std::string> *p) {
  std::ifstream ifs("ginboxchecker.conf");
  if(!ifs.good()) {
    printf("Could not open ginboxchecker.conf\n");
    ifs.close();
    return CURLE_READ_ERROR;
  }
  std::string line,name,value;
  std::getline(ifs,line);
  while(line.size()) {
    int ind = line.find("#"); //skip comments
    line = line.substr(0,ind);
    if(!line.size()) { std::getline(ifs,line); continue; }
    ind = line.find(";");     //skip everything after ;
    line = line.substr(0,ind);
    if(!line.size()) { std::getline(ifs,line); continue; }
    ind = line.find("=");
    name = line.substr(0,ind);
    value = line.substr(ind+2, line.size()-ind-3);
    (*p)[name]=value;
    std::getline(ifs,line);
  }
  bool wrong=false;
  if(!(*p)["email"].size()) {
     wrong = true;
     printf("email is missing.\n");
  }
  if(!(*p)["client_id"].size()) {
     wrong = true;
     printf("client_id is missing.\n");
  }
  if(!(*p)["client_secret"].size()) {
     wrong = true;
     printf("client_secret is missing.\n");
  }
  if(!(*p)["refresh_token"].size()) {
     wrong = true;
     printf("refresh_token is missing.\n");
  }
  if(wrong) {
     printf("Config file contains errors. Exiting..\n");
     ifs.close();
     return CURLE_READ_ERROR;
  }
  ifs.close();
  return CURLE_OK;
}

size_t writer( char *ptr, size_t size, size_t nmemb, void *userdata) {
   size_t realsize=size*nmemb;
   std::string *out = reinterpret_cast<std::string *>(userdata);
   out->append(ptr, realsize);
   return realsize;
}

CURLcode connectImap(std::string *buffer, std::map<std::string,std::string> *p) {
  CURL *curl;
  CURLcode res = CURLE_OK;
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, (*p)["email"].c_str());
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, (*p)["bearer"].c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "imaps://imap.gmail.com:993/INBOX");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "SEARCH UNSEEN");
    res = curl_easy_perform(curl);
  }
  curl_easy_cleanup(curl);
  return res;
}

CURLcode refreshToken(std::map<std::string, std::string> *p) {
  CURL *curl;
  CURLcode res = CURLE_OK;
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.google.com/o/oauth2/token");
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    std::stringstream cmd;
    cmd<<"client_id="<<(*p)["client_id"];
    cmd<<"&client_secret="<<(*p)["client_secret"];
    cmd<<"&refresh_token="<<(*p)["refresh_token"];
    cmd<<"&grant_type=refresh_token";
    std::string postcmd=cmd.str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postcmd.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    std::string buffer;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    res = curl_easy_perform(curl);
    if(!res) {
      std::stringstream b(buffer);
      std::string tmp;
      b>>tmp;
      while(tmp!=":") {
        b>>tmp;
      }
      b>>tmp;
      (*p)["bearer"]=tmp.substr(1,tmp.size()-3);
    }
  }
  curl_easy_cleanup(curl);
  return res;
}

void notifyByVoice(int ctr) {
  std::stringstream cmd;
  if(ctr>0) {
    cmd<<"say \"You have "<<ctr<<" new message"<<(ctr>1?"s":"")<<"!\"";
    std::string voicecmd = cmd.str();
    system(voicecmd.c_str());
  }
}

void notifyByBanner(int ctr) {
  std::stringstream cmd;
  if(ctr>0) {
    cmd<<"terminal-notifier -message ";
    cmd<<"'Gmail: "<<ctr<<" new message"<<(ctr>1?"s":"")<<"'";
    cmd<<" -title 'ginboxchecker' -open https://mail.google.com";
    std::string notifcmd = cmd.str();
    system(notifcmd.c_str());
  }
}
 
int main(void)
{
  std::string str;
  CURLcode res;
  std::map<std::string, std::string> properties;
  time_t nextrefresh = 0;
  time_t tnow;
  res = parseConfig(&properties);
  if(res) {
    printf("Could not parse the ginboxchecker.conf file. Exiting..\n");
    return (int)res;
  }
  while(true) {
    tnow = time(0);
    if(tnow > nextrefresh) {
      nextrefresh = tnow + 3300;
      res = refreshToken(&properties);
      if(res) {
        printf("Could not refresh the token. Exiting..\n");
        return (int)res;
      }
    }
    res = connectImap(&str, &properties);
    if(res) {
      printf("Could not connect to IMAP.\n");
      return (int)res;
    }
    int tmp;
    int ctr=0;
    std::stringstream lineStream(str);
    lineStream>>str;
    lineStream>>str;
    while(lineStream >> tmp) {
      ++ctr;
    }
    std::thread thr1(notifyByBanner,ctr);
    std::thread thr2(notifyByVoice,ctr);
    thr1.join();
    thr2.join();
    sleep(180);
    res = connectImap(&str, &properties);
  }
  return (int)res;
}
