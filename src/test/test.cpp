#include <iostream>
#include <cstring>
#include <string>
#include <cstdlib>

#include "../cping/cping.h"

using namespace std;

int main(int argc, char ** argv) {
    
    if (argc != 2) {
        cerr<<"usage : cping <#ip>"<<endl;
        return -1;
    }


    string strWWW = argv[1];
    CPing * cping = CPing::getInstance();
    cping->initialize(strWWW);
    cping->start();


    return 0;
}