#include<bits/stdc++.h>
using namespace std;
#define ll long long int

//Each unit of estab contains the following 
struct estabUnit {
  long long int address;//address
  long long int length;//length
  int isSymbol;
};

//for savning memory blocks 
struct code {
  string w;
  int addr;
};

//handling modiifcation record
struct handleM {
  long long int loc;
  long long int len;
  string name;
};

long long int startingAddress = 0;
map < string, estabUnit > ESTAB;
vector < code > final(100);
vector < handleM > v;


//converting hexadecimal to decimal
long long int convertHextoDec(string s) {
  int n = s.length() - 1;
  long long int j = 1, ans = 0;
  while (n >= 0) {
    if (s[n] >= '0' && s[n] <= '9') {
      ans += j * (s[n] - '0');
    } else if (s[n] >= 'A' && s[n] <= 'F') {
      ans += j * (s[n] - 'A' + 10);
    } else if (s[n] >= 'a' && s[n] <= 'f') {
      ans += j * (s[n] - 'a' + 10);
    }
    n--;
    j *= 16;
  }
  
  return ans;
}
string progname;

//converting decimla to hexadecimal
string convertDectoHex(long long int num) {

  ostringstream out;
  out << hex << num;
  return out.str();
}

ll csaddr;

//**********************************************************PASS 1*****************************************************************
int pass1(string inputFile) {
  fstream fProg;
  fProg.open(inputFile, ios:: in );
  //this is technically taken from os
  cout << "TYPE THE ADDRESS WHERE THE PROGRAM IS TO BE LOADED (in Decimal)\n";
  long long int progaddr;
  cin >> progaddr;
  //cout<<progaddr;
  fstream fwrite;
  //open loadmap
  fwrite.open("loadmap.txt", ios::out);
  csaddr = progaddr;
  string s;
  ll cslth = 0;

  while (getline(fProg, s)) {
    if (s.length()) {
      char c = s[0];
      if (c == 'H') {
        //edit estab in case it is a header record 
        progname = s.substr(1, 6);
        startingAddress = convertHextoDec(s.substr(7, 6));
        cslth = convertHextoDec(s.substr(13, 6));
        
        ESTAB[s.substr(1, 6)].address = csaddr;
        ESTAB[s.substr(1, 6)].length = cslth;
        ESTAB[s.substr(1, 6)].isSymbol = 0;
		//write to loadmap
        fwrite << s.substr(1, 6) << "\t" << setfill('0') << setw(5) << hex << csaddr << "\t" << setfill('0') << setw(5) << hex << cslth << endl;
        while (getline(fProg, s)) {
          char c = s[0];
          if (c == 'D') {
			//edit estab in define records too
            int i = 1;
            while (i < s.length()) {
              string name = s.substr(i, 6);
              i += 6;
              ll addr = convertHextoDec(s.substr(i, 6));
              i += 6;
              //cout<<name<< hex<<csaddr+addr<<endl;
              ESTAB[name].address = addr + csaddr;
              ESTAB[name].isSymbol = 1;
              fwrite << name << "\t" << setfill('0') << setw(5) << hex << addr + csaddr << "\t*" << endl;
            }
          }
		  //if end is reached, start reading other programs
          if (c == 'E') break;
        }

      }
	  //edit csaddr after end of program
      csaddr += cslth;
    }

  }
  cout << "PASS1 COMPLETED" << endl;

  return progaddr;
}

//**************************************************************PASS 2********************************************************

bool pass2(string inputFile, ll progaddr) {

  //this tells the max possible address which will be filled
  ll sizeofmem = ((csaddr) / 16) + (int)((bool) csaddr % 16);
  sizeofmem *= 32;

  //make a string and later on edit ot
  string Memblock(2 * sizeofmem, '.');
  ll startingAddress;
  string label;
  ll execaddr = progaddr;
  csaddr = progaddr;
  ifstream fin;
  fin.open(inputFile);
  ofstream fout;
  //memory display here 
  fout.open("output.txt");
  string s;
  ll cslth = 0;
  while (getline(fin, s)) {
    if (s.length()) {

      char st = s[0];
      if (st == 'H')
	    //save length of program in case of a new program
        cslth = convertHextoDec(s.substr(13, 6));
      else if (st == 'T') {
		//edit memory in case of a text record
        string c = s.substr(9);
        startingAddress = convertHextoDec(s.substr(1, 6));
        startingAddress += csaddr;
        startingAddress *= 2;
        for (int i = startingAddress; i < startingAddress + c.length(); i++) {
          Memblock[i] = c[i - startingAddress];
        }

      } else if (st == 'M') {
		//edit memory in case of a modification record 
        label = s.substr(9);
        if (ESTAB.find(label.substr(1)) != ESTAB.end()) {
          int mlength = convertHextoDec(s.substr(7, 2));
          ll val = ESTAB[label.substr(1)].address;
          ll maddress = convertHextoDec(s.substr(1, 6));
          maddress += csaddr;
          maddress *= 2;
          if (mlength & 1) maddress++;
          string val2;
          val2 = Memblock.substr(maddress, mlength);
          ll idk = 0;
          if (label[0] == '-') {
            idk = convertHextoDec(val2) - val;
          } else {
            idk = convertHextoDec(val2) + val;
          }
          ostringstream out;
          out << setfill('0') << setw(mlength) << hex << idk;
          val2 = out.str();
          for (int i = 0; i < mlength; i++) {
            Memblock[i + maddress] = val2[i];
          }
        } else {
          return 0;
        }

      } else if (st == 'E') {
		//break if end is reached
        if (s.length() > 1) {
          execaddr = csaddr;
          execaddr += convertHextoDec(s.substr(1));
        }
        csaddr += cslth;
      }
    }

  }

  ll idk = 0;

	//printing the memory block in files
  for (int i = 0; i < sizeofmem; i++) {
    if (i % 32 == 0 && i) fout << endl;
    if (i % 32 == 0) {
      fout << setfill('0') << setw(6) << hex << idk << '\t';
      idk += 16;
    }
    if (i % 8 == 0 && i % 32) {
      fout << " ";
    }
    fout << Memblock[i];
  }
  cout << "PASS 2 COMPLETED" << endl;
  fin.close();
  fout.close();
  return 1;
}

int main() {
  string inputFile;

  inputFile = "inp.txt";

  int progaddr = pass1(inputFile);

  pass2(inputFile, progaddr);

}