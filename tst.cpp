#include <iostream>


int main(){

   std::cout << "\033[H\033[2J";
 while(true){
    //
    std::cout << "----------------------------------------------\n";

   std::cout << "\033[100;3H";
     std::cout << "\r\033[2K";
    char test[2000] = {0};
    std::cin.getline(test, sizeof(test));
    std::cout << "\033[5A";
    std::cout << "string is : " << test << std::endl;
  }
  return 0;
}
