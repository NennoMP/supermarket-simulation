# Operating Systems and Laboratory Project

Project for Operating Systems and Laboratory course at the University of Pisa. The project aim is to realize a simulation of a system that models a supermarket. The supermarket behaviour depens on some default values that can be found in `./config/config.txt`, the explanation of what they represent is in `./Readme/readme.txt`. 

The supermarket.c process simulates cashiers and customers behaviour with a multithread implementation and communication between `supermarket.c` and `and manager.c` processes is implemented with an AF_UNIX socket.
