# Two-Pass-Assembler


Run the commands in the folder contaniing the code and the input file - inp.txt


###Assembler.cpp contains program for the assembler for sic xe with provisions for the extended version instructions in the code.

TO COMPILE:

 g++ -w Assembler.cpp


TO RUN:

 ./a.out


 
pass1 creates the intermediate.txt

pass2 creates the object.txt
 
 
###LinkerLoader.cpp contains program for the two pass linker loader

TO COMPILE:

 g++ -w LinkerLoader.cpp
 
 
TO RUN:

 ./a.out


pass 1 gives the loadmap.txt

pass2 gives the output.txt with the memory contents as output as shown in the book

