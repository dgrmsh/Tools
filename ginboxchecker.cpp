#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <unistd.h>
#include </usr/local/include/curl/curl.h>

CURLcode parseConfig(std::string *Email, std::string *ClientId, std::string *ClientSecret, std::string *RefreshToken) {
  std::ifstream ifs("ginboxchecker.conf");
  int lnumber=1;
  if(!ifs.good()) {
    printf("Could not open ginboxchecker.conf\n");
    ifs.close();
    return CURLE_READ_ERROR;
  }
  std::string line,name,value;
  std::getline(ifs,line);
  while(line.size()) {
    int ind = line.find("=");
    name = line.substr(0,ind);
    value = line.substr(ind+2, line.size()-ind-4);
    if(name.size()>0 && value.size()>0) {
      if(name == "email") {
        *Email = value;
      } else if(name == "client_id") {
        *ClientId = value;
      } else if(name == "client_secret") {
        *ClientSecret = value;
      } else if(name == "refresh_token") {
        *RefreshToken = value;
      } else {
        printf("Unknown parameter name in line '%s'\n",line.c_str());
        ifs.close();
        return CURLE_READ_ERROR;
      }
    } else {
      printf("Line %d:'%s' is not of form name=value;\n",lnumber,line.c_str());
      ifs.close();
      return CURLE_READ_ERROR;
    }
    std::getline(ifs,line);
    ++lnumber;
  }
  bool wrong=false;
  if(!Email->size()) {
     wrong = true;
     printf("email is missing.\n");
  }
  if(!ClientId->size()) {
     wrong = true;
     printf("client_id is missing.\n");
  }
  if(!ClientSecret->size()) {
     wrong = true;
     printf("client_secret is missing.\n");
  }
  if(!RefreshToken->size()) {
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

CURLcode connectImap(std::string *buffer, std::string *Email, std::string *Bearer) {
  CURL *curl;
  CURLcode res = CURLE_OK;
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, Email->c_str());
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, Bearer->c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "imaps://imap.gmail.com:993/INBOX");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "SEARCH UNSEEN");
    res = curl_easy_perform(curl);
  }
  curl_easy_cleanup(curl);
  return res;
}

CURLcode refreshToken(std::string *ClientId, std::string *ClientSecret, std::string *RefreshToken, std::string *Bearer) {
  CURL *curl;
  CURLcode res = CURLE_OK;
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.google.com/o/oauth2/token");
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    std::stringstream cmd;
    cmd<<"client_id="<<*ClientId<<"&client_secret="<<*ClientSecret;
    cmd<<"&refresh_token="<<*RefreshToken<<"&grant_type=refresh_token";
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
      *Bearer=tmp.substr(1,tmp.size()-3);
    }
  }
  curl_easy_cleanup(curl);
  return res;
}

void notifyByVoice(int ctr) {
  std::stringstream cmd;
  if(ctr>0) {
    cmd<<"say \"You have "<<ctr<<" new message"<<(ctr>1?"s":"")<<"!\"";
  }
  system(cmd.str().c_str());
}
 
int main(void)
{
  std::string str, Email, ClientId, ClientSecret, RefreshToken, Bearer;
  CURLcode res;
  res = parseConfig(&Email, &ClientId, &ClientSecret, &RefreshToken);
  if(res) {
    printf("Could not parse the ginboxchecker.conf file. Exiting..\n");
    return (int)res;
  }
  res = refreshToken(&ClientId, &ClientSecret, &RefreshToken, &Bearer);
  if(res) {
    printf("Could not refresh the token. Exiting..\n");
    return (int)res;
  }
  res = connectImap(&str, &Email, &Bearer);
  while(!res) {
    int tmp;
    int ctr=0;
    std::stringstream lineStream(str);
    lineStream>>str;
    lineStream>>str;
    while(lineStream >> tmp) {
      ++ctr;
    }
    notifyByVoice(ctr);
    sleep(300);
    res = connectImap(&str, &Email, &Bearer);
  }
  if(res) {
    printf("Could not connect to IMAP.\n");
  }
  return (int)res;
}
