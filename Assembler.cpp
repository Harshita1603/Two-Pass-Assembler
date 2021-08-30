#include <bits/stdc++.h>
using namespace std;

map<string, string> OPTAB = {  //Opcode Table for Required Instructions.
	{"LDA", "00"},
	{"LDX", "04"},
	{"LDL", "08"},
	{"LDB", "68"},
	{"LDT", "74"},
	{"STA", "0C"},
	{"STX", "10"},
	{"STL", "14"},
	{"LDCH", "50"},
	{"STCH", "54"},
	{"ADD", "18"},
	{"SUB", "1C"},
	{"MUL", "20"},
	{"DIV", "24"},
	{"COMP", "28"},
	{"COMPR", "A0"},
	{"CLEAR", "B4"},
	{"J", "3C"},
	{"JLT", "38"},
	{"JEQ", "30"},
	{"JGT", "34"},
	{"JSUB", "48"},
	{"RSUB", "4C"},
	{"TIX", "2C"},
	{"TIXR", "B8"},
	{"TD", "E0"},
	{"RD", "D8"},
	{"WD", "DC"}};

map<string, string> REGTAB = {	//Table for storing Register Numbers
	{"A", "0"},
	{"X", "1"},
	{"L", "2"},
	{"B", "3"},
	{"S", "4"},
	{"T", "5"},
	{"F", "6"},
	{"PC", "8"},
	{"SW", "9"}};

set<string> twoByte{"COMPR", "CLEAR", "TIXR"};	//Extension of OPTAB to specify which instructions are 2 byte

struct codeLine {	  //Width (Assuming single column for space in between them)
	string label;	  //10 Columns
	string opcode;	  //10 Columns
	string operands;  //30 Columns
	char preOpcode;
	char preOperand;
};

string pad(string, int);

class objCode {	 // Class storing complete information about Format 3 and Format 4 Object codes.
public:
	string opcode;	// 8bit Opcode
	bool n;			// Indirect Flag
	bool i;			// Immediate addressing flag
	bool x;			// Indexed Addressing flag
	bool b;			// Base-indexing flag
	bool p;			// PC - indexing flag
	bool e;			// Extended Format flag
	int target;

	objCode(string op) {
		opcode = OPTAB[op];
		n = 1;
		i = 1;
		x = 0;
		b = 0;
		p = 1;
		e = 0;
	}

	string getObj() {
		int instruction;
		istringstream(opcode) >> hex >> instruction;
		bitset<8> bin(instruction);
		string obj = bin.to_string().substr(0, 6);	//Getting first 6-bits of Opcode
		obj += bool2str(n);
		obj += bool2str(i);
		obj += bool2str(x);
		obj += bool2str(b);
		obj += bool2str(p);
		obj += bool2str(e);

		ostringstream tar;
		tar << hex << target;
		string temp;
		int len = (e ? 5 : 3);
		if (target >= 0) {	//target can be positive
			temp = string(len - tar.str().size(), '0');
			temp += tar.str();
		} else {  //target can be negative.
			temp = tar.str();
			temp = temp.substr(temp.size() - len, len);	 //C++ handles 2'complement conversion by default.
		}

		bitset<12> binary(obj);
		ostringstream out;
		out << hex << binary.to_ulong();
		string str = ::pad(out.str(), 3);
		str += temp;  //Object Code ready
		return str;
	}

	int getLen() {
		return (e ? 4 : 3);	 //Length of the Instruction (Bytes)
	}

	string bool2str(bool flag) {
		return (flag ? "1" : "0");
	}
};

vector<map<string, int>> SYMTAB(1);	 //Symbol Table to map Labels to Addresses
vector<map<string, int>> LITTAB(1);	 //Literal Table
vector<set<string>> extrefs(1);		 //Store External References of the section
vector<vector<int>> relLocs(1);
int startAddr;
vector<int> programLength(0);  //Program Lengths of Individual Sections
bool Error;

int computeLength(const string&);
void read(const string&, codeLine&);
void prune(string&);
void capitalize(string&);
void passOne();
void passTwo();

void makeHeader(ofstream&, codeLine&, int);
void makeDefs(ofstream&, codeLine&, int);
void makeRefs(ofstream&, codeLine&, int);
void makeModRecs(ofstream&, string&);
void writeRecord(ofstream&, objCode&, string&, string&, int&);
void writeRecordTwo(ofstream&, string&, int, string&, string&, int&);
void endRec(ofstream&, string&, int);
void makeEndRec(ofstream&, int);
int evalExpression(string&, int, string&, int);
void dumpLiterals(ofstream&, int, int&);

bool blank(const string&);
void split(vector<string>&, const string&);

int main() {
	Error = false;

	if (!Error) passOne();
	if (!Error) passTwo();

	return 0;
}

void passOne() {  //Begin first pass
	ifstream fin;
	ofstream fout;
	string currLine;
	codeLine currCode;
	int LOCCTR, jump;
	int controlSect;

	fout.open("Intermediate.txt");
	fin.open("inp.txt");
	if (!fin) {
		Error = true;
		cout << "Error: Source Code not found" << endl;
		return;
	}
	getline(fin, currLine);	 //get first line
	read(currLine, currCode);

	if (currCode.opcode == "START") {  //Initialize everything
		controlSect = 0;
		istringstream(currCode.operands) >> hex >> startAddr;
		ostringstream loc;

		LOCCTR = startAddr;
		loc << hex << LOCCTR;
		relLocs[controlSect].push_back(LOCCTR);
		fout << pad(loc.str(), 4) << " " << currLine << endl;

		getline(fin, currLine);
		read(currLine, currCode);
	} else
		LOCCTR = 0;	 // Address 0 if no other address defined

	while (currCode.opcode != "END") {
		jump = 0;
		bool write = true;
		bool comment = false;
		string Opcode = currCode.opcode;
		if (currCode.label != ".") {
			if (currCode.label != "") {
				if (SYMTAB[controlSect].count(currCode.label)) {
					Error = true;
					cout << "Error: duplicate label" << endl;
					return;
				} else if (Opcode != "CSECT")
					SYMTAB[controlSect][currCode.label] = LOCCTR;  //Store in Symbol Table
			}

			if (currCode.preOperand == '=')	 //Check for Literals
				if (!LITTAB[controlSect].count(currCode.operands))
					LITTAB[controlSect].insert({currCode.operands, -1});  //Insert in LITTAB with unassigned address

			if (OPTAB.count(Opcode)) {
				int instructionLength = (currCode.preOpcode == '+' ? 4 : 3);  //Check Extended format

				if (twoByte.count(Opcode)) instructionLength = 2;
				jump = instructionLength;
			} else if (Opcode == "WORD")
				jump = 3;				  //Word Operand always has length 3
			else if (Opcode == "RESW") {  //RESW and RESB have Operands in Decimal
				int dec_operand;
				istringstream(currCode.operands) >> dec >> dec_operand;
				jump = 3 * dec_operand;
			} else if (Opcode == "RESB") {
				int dec_operand;
				istringstream(currCode.operands) >> dec >> dec_operand;
				jump = dec_operand;
			} else if (Opcode == "BYTE") {
				jump = computeLength(currCode.operands);  //Special Subroutine to Calculate length for BYTE
			} else if (Opcode == "LTORG") {
				fout << "     " << currLine << endl;
				dumpLiterals(fout, controlSect, LOCCTR);  //Assign addresses to literals and print to Intermediete
				write = false;
			} else if (Opcode == "EXTREF" || Opcode == "EXTDEF") {
				fout << "     " << currLine << endl;
				write = false;
			} else if (Opcode == "EQU") {
				jump = 0;
				string operand = currCode.operands;
				string val;
				if (operand == "*")
					write = true;								// * means current location pointer
				else if (SYMTAB[controlSect].count(operand)) {	// If Operand is already a Symbol
					ostringstream loc;
					loc << hex << SYMTAB[controlSect][operand];
					int LOCCTRR = SYMTAB[controlSect][operand];
					SYMTAB[controlSect][currCode.label] = LOCCTR;
					if (relLocs[controlSect].back() != LOCCTRR)
						relLocs[controlSect].push_back(LOCCTRR);
					fout << pad(loc.str(), 4) << " " << currLine << endl;

					write = false;
				} else {  //If Operand is an expression
					string temp = "-1";
					int val = evalExpression(currCode.operands, controlSect, temp, 0);	//Computes the value of expression
					SYMTAB[controlSect][currCode.label] = val;
					ostringstream out;
					out << hex << val;
					fout << pad(out.str(), 4) << " " << currLine << endl;
					write = false;
				}
			} else if (Opcode == "CSECT") {				  //Start of new section
				dumpLiterals(fout, controlSect, LOCCTR);  //Dump literals of previous section
				programLength.push_back(LOCCTR - startAddr);

				startAddr = 0;	//Reset Start Address
				LOCCTR = 0;		//Reset Location Counter
				jump = 0;
				controlSect++;	//Increment Control Section Number
				map<string, int> newSYM, newLIT;
				vector<int> newLocs;
				relLocs.push_back(newLocs);	 //New Location Array
				SYMTAB.push_back(newSYM);	 //New Symbol Table
				LITTAB.push_back(newLIT);	 //New Literal Table
				write = true;
			} else {
				Error = true;
				cout << "Error: invalid opcode" << endl;
				return;
			}
		} else
			comment = true;

		if (write) {
			ostringstream loc;

			loc << hex << LOCCTR;
			if (relLocs[controlSect].empty() || relLocs[controlSect].back() != LOCCTR)
				relLocs[controlSect].push_back(LOCCTR);
			fout << (!comment ? pad(loc.str(), 4) : string(4, ' ')) << " " << currLine << endl;	 //Write line along with address to Intermediate file
			if (currLine[0] != '.') LOCCTR += jump;
		}
		getline(fin, currLine);	 //read next line
		read(currLine, currCode);
	}

	ostringstream loc;
	loc << hex << LOCCTR;
	if (relLocs[controlSect].back() != LOCCTR)
		relLocs[controlSect].push_back(LOCCTR);
	fout << pad(loc.str(), 4) << " " << currLine << " " << endl;  //write last line

	dumpLiterals(fout, controlSect, LOCCTR);  //dump literals of last section

	programLength.push_back(LOCCTR - startAddr);

	fin.close();   //close Source Code.txt
	fout.close();  //close Intermediete.txt
}

void passTwo() {  //Beginning of Pass Two
	ifstream fin;
	ofstream fout;
	string currLine;
	codeLine currCode;
	string currRecord;
	string modRecord;
	string temp, object;
	string LOC;
	bool res = false;
	int locIdx;
	//bool to specify if previous command was RESB or RESW

	int controlSect;
	fout.open("Object Program.txt");  //File to store final object program
	fin.open("Intermediate.txt");	  //Open Intermediate produced by pass one.
	if (!fin) {
		Error = true;
		cout << "Error: Intermediate file not found" << endl;
		return;
	}
	getline(fin, currLine);
	LOC = currLine.substr(0, 4);
	read(currLine.substr(5, currLine.size() - 5), currCode);

	if (currCode.opcode == "START") {
		controlSect = 0;
		istringstream(currCode.operands) >> hex >> startAddr;
		makeHeader(fout, currCode, controlSect);
	}
	getline(fin, currLine);
	string tempLOC = currLine.substr(0, 4);
	if (!blank(tempLOC)) LOC = tempLOC;
	read(currLine.substr(5, currLine.size() - 5), currCode);
	int currLoc, nextLoc, incr;
	istringstream(LOC) >> hex >> currLoc;  //Write Header to Output
	locIdx = 0;

	cout << LOC << endl;
	currRecord = "T" + pad(LOC, 6) + "00";	//Initiate Record with Start address and Zero length.
	int recLength = 0;
	while (1) {
		if (currCode.label != ".") {
			string Opcode = currCode.opcode;
			string Operand = currCode.operands;
			string objectCode;
			if (Opcode == "EXTDEF")
				makeDefs(fout, currCode, controlSect);
			else if (Opcode == "EXTREF")
				makeRefs(fout, currCode, controlSect);
			else if (OPTAB.count(Opcode)) {
				object = OPTAB[Opcode];
				temp = "";

				cout << Opcode << " " << Operand << " " << currLoc << " " << nextLoc << endl;
				if (Operand != "") {
					if (SYMTAB[controlSect].count(Operand)) {  // Normal Instructions where Operand is a symbol
						objCode obj(Opcode);				   // and Opcode is in OPTAB
						if (currCode.preOpcode == '+') {	   // check extended format
							obj.e = true;					   // set extension flag
							obj.p = false;					   // b=0 and p=0 always for format 4
						}
						if (currCode.preOperand == '@') obj.i = false;		  // check indirect addressing
						obj.target = SYMTAB[controlSect][Operand] - nextLoc;  // calculate displacement

						writeRecord(fout, obj, currRecord, LOC, recLength);	 // subroutine to write to record
						locIdx++;
					} else if (extrefs[controlSect].count(Operand)) {  // If operand is an external reference
						objCode obj(Opcode);
						if (currCode.preOpcode == '+') {
							obj.e = true;
							obj.p = false;
						}
						obj.target = 0;
						ostringstream out;
						out << hex << (currLoc + 1);
						modRecord += "M" + pad(out.str(), 6) + (obj.e ? "05" : "03") + "+" + Operand + "\n";
						writeRecord(fout, obj, currRecord, LOC, recLength);
						locIdx++;
					} else if (currCode.preOperand == '#') {  // Check Immediete Operands
						objCode obj(Opcode);
						int arg;
						istringstream(Operand) >> hex >> arg;
						obj.p = false;
						obj.n = false;
						obj.target = arg;

						writeRecord(fout, obj, currRecord, LOC, recLength);
						locIdx++;
					} else if (currCode.preOperand == '=') {  // check if Operand is a Literal
						objCode obj(Opcode);
						obj.target = LITTAB[controlSect][Operand] - nextLoc;

						writeRecord(fout, obj, currRecord, LOC, recLength);
						locIdx++;
					} else if (twoByte.count(Opcode)) {	 // 2 byte instruction
						bool comma = 0;
						for (auto x : Operand)
							if (x == ',') comma = 1;

						objectCode = OPTAB[Opcode];
						if (!comma)
							objectCode += (REGTAB[Operand] + "0");
						else {
							vector<string> regs;
							split(regs, Operand);
							objectCode += (REGTAB[regs[0]] + REGTAB[regs[1]]);
						}

						incr = 2;
						writeRecordTwo(fout, objectCode, incr, currRecord, LOC, recLength);
						locIdx++;
					} else if (Opcode == "STCH" || Opcode == "LDCH") {	//Here Operand will be of form OPERAND,X
						objCode obj(Opcode);
						obj.x = true;
						if (currCode.preOpcode == '+') {
							obj.e = true;
							obj.p = false;
						}
						vector<string> labels;
						split(labels, Operand);
						if (SYMTAB[controlSect].count(labels[0]))  // If Label from current section
							obj.target = SYMTAB[controlSect][labels[0]] + 0x8000;
						else if (extrefs[controlSect].count(labels[0])) {  // If label is externally referenced.
							obj.target = 0;
							ostringstream out;
							out << hex << (currLoc + 1);
							modRecord += "M" + pad(out.str(), 6) + (obj.e ? "05" : "03") + "+" + labels[0] + "\n";
						}

						writeRecord(fout, obj, currRecord, LOC, recLength);
						locIdx++;
					} else {
						Error = true;
						// cout<<"Error: undefined symbol"<<endl;
						// return;
						temp = "xxxx";
					}
				} else {  // For cases like "RSUB"
					objCode obj(Opcode);
					obj.target = 0;
					obj.p = false;
					writeRecord(fout, obj, currRecord, LOC, recLength);
					locIdx++;
				}
			} else if (currCode.preOpcode == '=') {	 // Dumped Literal Definition
				Opcode += '\'';
				if (LITTAB[controlSect].count(Opcode)) {
					if (Opcode[0] == 'C' || Opcode[0] == 'c' && Opcode[1] == '\'') {
						temp = "";
						ostringstream out;
						for (int i = 2; Opcode[i] != '\''; i++) out << hex << (int)Opcode[i];
						objectCode = out.str();
					}

					if (Opcode[0] == 'X' || Opcode[1] == 'x' && Opcode[1] == '\'')
						objectCode = Opcode.substr(2, 2);

					incr = computeLength(Opcode);
					writeRecordTwo(fout, objectCode, incr, currRecord, LOC, recLength);
					locIdx++;
				} else
					cout << "Unknown Literal" << endl;
			} else if (Opcode == "WORD") {											 // Opcode is WORD
				int val = evalExpression(Operand, controlSect, modRecord, currLoc);	 // Operand can be an expression
				ostringstream out;													 // Can also add to Modification records
				out << hex << val;
				objectCode = pad(out.str(), 6);

				incr = 3;
				writeRecordTwo(fout, objectCode, incr, currRecord, LOC, recLength);
				locIdx++;
			} else if (Opcode == "BYTE") {	// BYTE Operands have a different format
				if (Operand[0] == 'C' || Operand[0] == 'c' && Operand[1] == '\'') {
					temp = "";
					ostringstream out;
					for (int i = 2; Operand[i] != '\''; i++) out << hex << (int)Operand[i];
					objectCode = out.str();
				}

				if (Operand[0] == 'X' || Operand[1] == 'x' && Operand[1] == '\'')
					objectCode = Operand.substr(2, 2);

				incr = computeLength(Operand);
				writeRecordTwo(fout, objectCode, incr, currRecord, LOC, recLength);
				locIdx++;
			} else if (Opcode == "RESW" || Opcode == "RESB") {	// These iinstructions end current record line
				endRec(fout, currRecord, recLength);
				recLength = 0;
				currRecord = "T";  // Start new record
				locIdx++;
			} else if (Opcode == "CSECT") {			  // Beginning of new section
				endRec(fout, currRecord, recLength);  // End old record
				makeModRecs(fout, modRecord);		  // Print Modification Records
				makeEndRec(fout, controlSect);		  // Print End record

				set<string> newSet;
				extrefs.push_back(newSet);				  // New External references collection
				controlSect++;							  // Go to next section
				startAddr = 0;							  // Reset start address
				modRecord.clear();						  // Clear Modification Records
				locIdx = 0;								  // Reset Location index
				currRecord = "T";						  // New Record
				recLength = 0;							  // Reset record length
				makeHeader(fout, currCode, controlSect);  // Make header for the new section
			}
		}

		getline(fin, currLine);		  // Get next line
		if (currLine.empty()) break;  // Break out of loop at EOF
		string tempLOC = currLine.substr(0, 4);
		if (!blank(tempLOC)) LOC = tempLOC;	 // read Location
		istringstream(LOC) >> hex >> currLoc;
		while (locIdx < relLocs[controlSect].size() && currLoc != relLocs[controlSect][locIdx]) locIdx++;  // Bring location index to position
		if (locIdx + 1 < relLocs[controlSect].size()) nextLoc = relLocs[controlSect][locIdx + 1];		   // Get next location
		read(currLine.substr(5, currLine.size() - 5), currCode);										   // Read line, except the Address
	}
	endRec(fout, currRecord, recLength);
	makeModRecs(fout, modRecord);
	makeEndRec(fout, controlSect);

	fin.close();
	fout.close();
}

void read(const string& line, codeLine& code) {	 // Function to read current line
	if (line[0] == '.' || line.size() == 1 || line.size() == 0) {
		code.label = ".";
		code.opcode = "";
		code.operands = "";
		return;
	}

	string temp;
	temp = line.substr(0, 9);
	prune(temp);
	code.label = temp;

	code.preOpcode = line[9];
	temp = line.substr(10, min(9, (int)line.size() - 11));
	prune(temp);
	code.opcode = temp;

	if (line.size() > 20) {
		code.preOperand = line[19];
		temp = line.substr(20, (int)line.size() - 21);
		prune(temp);
		code.operands = temp;
	} else
		code.operands = "";
}

void prune(string& str) {  // Prune entries from the file for blank spaces
	int pause = str.size();
	for (int i = 0; i < str.size(); i++) {
		if (str[i] == ' ') {
			pause = i;
			break;
		}
	}
	str = str.substr(0, pause);
}

int computeLength(const string& operand) {	// Length of Operands like C'EOF'
	int len;
	if ((operand[0] == 'C' || operand[0] == 'c') && operand[1] == '\'') {
		for (len = 2; len < operand.size(); len++) {
			if (operand[len] == '\'') {
				len -= 2;
				break;
			}
		}
	}

	if ((operand[0] == 'X' || operand[1] == 'x') && operand[1] == '\'')
		len = 1;

	return len;
}

void capitalize(string& str) {	// C++ stores hexadecimals in lower case
	for (auto& x : str)
		if (x >= 'a' && x <= 'z') x = x - 'a' + 'A';
}

string pad(string str, int len) {  // make string of appropriate length
	string temp(len - str.size(), '0');
	temp += str;
	return temp;
}

void makeHeader(ofstream& fout, codeLine& currCode, int controlSect) {	// Make Header record
	ostringstream out;
	string currRecord = "H" + currCode.label;
	while (currRecord.size() < 7) currRecord += " ";

	out << hex << startAddr;
	string temp = out.str();
	for (int i = 0; i < 6 - temp.size(); i++) currRecord += "0";
	currRecord += temp;

	out.str("");
	out.clear();
	out << hex << programLength[controlSect];
	temp = out.str();
	for (int i = 0; i < 6 - temp.size(); i++) currRecord += "0";
	currRecord += temp;

	capitalize(currRecord);
	fout << currRecord << endl;
}

void makeDefs(ofstream& fout, codeLine& currCode, int controlSect) {  // Make Definition Record
	vector<string> labels;
	split(labels, currCode.operands);
	string currRecord = "D";
	for (auto& x : labels) {
		ostringstream out;
		out << hex << SYMTAB[controlSect][x];
		string temp(6 - out.str().size(), '0');
		temp += out.str();

		while (x.size() < 6) x += " ";
		currRecord = currRecord + x + temp;
	}
	capitalize(currRecord);
	fout << (currRecord) << endl;
}

void makeRefs(ofstream& fout, codeLine& currCode, int controlSect) {  // Make reference Record
	vector<string> labels;
	split(labels, currCode.operands);
	string currRecord = "R";
	for (auto& x : labels) {
		extrefs[controlSect].insert(x);
		while (x.size() < 6) x += " ";
		currRecord += x;
	}
	fout << currRecord << endl;
}

void makeModRecs(ofstream& fout, string& out) {	 // Print Modification Records
	capitalize(out);
	fout << out;
}

void makeEndRec(ofstream& fout, int controlSect) {	// Print End Record
	string currRecord = "E";
	if (controlSect == 0) {
		ostringstream out;
		out << hex << startAddr;
		string temp(6 - out.str().size(), '0');
		temp += out.str();
		currRecord += temp;
	}
	fout << currRecord << endl
		 << endl;
}

void split(vector<string>& labels, const string& ops) {	 // Split Comma separated strings into Vectors
	int start = 0;
	for (int i = 0; i < ops.size(); i++) {
		if (ops[i] == ',') {
			labels.push_back(ops.substr(start, i - start));
			start = i + 1;
		}
	}
	labels.push_back(ops.substr(start, ops.size() - start));
}

void writeRecord(ofstream& fout, objCode& obj, string& currRecord, string& LOC, int& recLength) {  // Write Record Function-1
	if (currRecord == "T") currRecord += pad(LOC, 6) + "00";
	if (recLength + obj.getLen() > 30) {
		ostringstream out;
		out << hex << recLength;
		string temp = pad(out.str(), 2);
		currRecord[7] = temp[0];
		currRecord[8] = temp[1];
		capitalize(currRecord);
		fout << currRecord << endl;

		currRecord = "T" + pad(LOC, 6) + "00";
		recLength = 0;
	}
	currRecord += obj.getObj();
	recLength += obj.getLen();

	cout << obj.getObj() << " " << recLength << endl;
}

void writeRecordTwo(ofstream& fout, string& objectCode, int incr, string& currRecord, string& LOC, int& recLength) {  // Write Record Function-2
	if (currRecord == "T") currRecord += pad(LOC, 6) + "00";
	if (recLength + incr > 30) {
		ostringstream out;
		out << hex << recLength;
		string temp = pad(out.str(), 2);
		currRecord[7] = temp[0];
		currRecord[8] = temp[1];
		capitalize(currRecord);
		fout << currRecord << endl;

		currRecord = "T" + pad(LOC, 6) + "00";
		recLength = 0;
	}
	currRecord += objectCode;
	recLength += incr;

	cout << objectCode << " " << recLength << endl;
}

void endRec(ofstream& fout, string& currRecord, int recLength) {  // End current record
	if (currRecord == "T") return;
	ostringstream out;
	out << hex << recLength;
	string temp = pad(out.str(), 2);
	currRecord[7] = temp[0];
	currRecord[8] = temp[1];
	capitalize(currRecord);
	fout << currRecord << endl;
}

bool blank(const string& str) {	 // Check if string is blank
	for (auto x : str) {
		if (x != ' ') return 0;
	}
	return 1;
}

int evalExpression(string& Operand, int controlSect, string& modRecord, int currLoc) {	// Evaluate Expression
	vector<char> operations;
	vector<string> labels;
	operations.push_back('+');
	int start = 0;
	for (int i = 0; i < Operand.size(); i++) {
		if (Operand[i] == '+' || Operand[i] == '-') {
			labels.push_back(Operand.substr(start, i - start));
			operations.push_back(Operand[i]);
			start = i + 1;
		}
	}
	labels.push_back(Operand.substr(start, Operand.size() - start));

	int val = 0;
	for (int i = 0; i < labels.size(); i++) {
		if (SYMTAB[controlSect].count(labels[i])) {
			if (operations[i] == '+') val += SYMTAB[controlSect][labels[i]];
			if (operations[i] == '-') val -= SYMTAB[controlSect][labels[i]];
		} else if (extrefs[controlSect].count(labels[i])) {
			if (operations[i] == '+') {
				ostringstream out;
				out << hex << (currLoc);
				modRecord += "M" + pad(out.str(), 6) + ("06") + "+" + labels[i] + "\n";
			}
			if (operations[i] == '-') {
				ostringstream out;
				out << hex << (currLoc);
				modRecord += "M" + pad(out.str(), 6) + ("06") + "-" + labels[i] + "\n";
			}
		}
	}

	return val;
}

void dumpLiterals(ofstream& fout, int controlSect, int& LOCCTR) {  // Print and assign addresses to literals.
	for (auto& x : LITTAB[controlSect]) {
		if (x.second == -1) {
			ostringstream loc;
			loc << hex << LOCCTR;
			if (relLocs[controlSect].back() != LOCCTR)
				relLocs[controlSect].push_back(LOCCTR);
			string currLine = pad(loc.str(), 4);
			currLine += " *";
			currLine += string(8, ' ');
			currLine += "=";
			currLine += x.first;
			fout << currLine << endl;

			x.second = LOCCTR;
			int jump = computeLength(x.first);
			LOCCTR += jump;
		}
	}
}