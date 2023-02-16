#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
using namespace std;

int main(int argc, char *argv[]){
    ofstream fp;
    int n = atoi(argv[2]);
    for (int i=1; i<=n; i++){
        sleep(1);
        fp.open(argv[1]);
        fp << "Process ran " << i << " out of " << n << " secs" << endl;
        fp.close();
    }
    return 0;
}

