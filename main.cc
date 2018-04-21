#include <iostream>
#include <fstream>
#include <array>
#include <vector>
#include <ctype.h>
#include <bitset>

// generate list of tokens <name , value > e.g <"id0", "NOP" , line_no , mem_loc> , <"id1","JMP" , line_no , mem_loc> 
//<"DATA" , "00H" , line_no , mem_loc>
// evaluate whether the sequence of tokens is syntactically correct returns a data structure for code generation
// takes the above parser results and generates a pseudo binary code

/*
	Token list : id0 , id1 ',' , ';' , 'DATA' , 'ADDR' , A , B , C , D , E , H , L 
	
	a keyword string array for token ID0 = [],ID1 = []

	data inside keyword { name  , binary_code}

	for id0 register ; we have sepprate map table of opcode where binary code redirects to the map
	for id0 register,register ; we have sepprate map table of opcode where binary code redirects to the map
========================================================================

	token structure { class_id:e_value  , "VALUE" , line_no  , mem_loc }

	ENUM e [ID0 , ID1 , COMMA , EOL , DATA , ADDR , A , B , C , D , E , H , L]

========================================================================
	tokenised_source => list of tokens in sequence

	parser grammer rules

	id0: ; | R,R; | R; 
	id1: ADDR; | R,D; | D; | LABEL;

	the above are the accepted statements 
========================================================================
	For jump statements and call statements we set up labels
	
	LABEL: COLONid0 | COLONid1 

	LABEL=> [_,a-Z][-,a-Z,0-9]*
*/
//////////////////////////////////////////////// BASIC DATA STRUCTURES TO HOLD THE INSTRUCTION DATA ///////////////////////////
#define BINARY_WORD_SIZE 8

enum TOKEN_CLASS { ID0 , ID1 , DATA , ADDR , LABEL  , REGISTER='R' , COLON=':' , COMMA=',' , EOL=';' , SPACE=' ' , UKN=-1};

struct  keyword
{
	std::string name;
	unsigned char binary;
};

#define MOV_MAP 0xF1
#define ADD_MAP 0xF2
#define MVI_MAP 0xF3
#define JMP_MAP 0xFF

std::array<keyword , 4> ID0_LIST = {{
	{"NOP" ,  0x00},
	{"CMA"  , 0x2F},
	{"MOV", MOV_MAP},
	{"ADD" , ADD_MAP}
}};

std::array<keyword , 4> ID1_LIST = {{
	{"MVI",  MVI_MAP},
	{"STA" , 0x32},
	{"ADI" , 0xC6},
	{"JMP" , JMP_MAP}
}};

std::array<std::array<unsigned char,8>,8> MOV_TABLE={{
	{0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47},
	{0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F},
	{0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57},
	{0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F},
	{0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67},
	{0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F},
	{0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77},
	{0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F},
}};

std::array<unsigned char , 8> MVI_TABLE = {0x06,0x0E,0x16,0x1E,0x26,0x2E,0x36,0x3E};
std::array<unsigned char, 8> ADD_TABLE = {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87};


int get_register_pos(char reg)
{
	if(reg == 'A')
		return 7;
	else if(reg == 'M')
		return 6;
	else if (reg == 'L')
		return 5;
	else if (reg == 'H')
		return 4;
	else if(reg == 'E')
		return 3;
	else if (reg == 'D')
		return 2;
	else if (reg == 'C')
		return 1;
	else if (reg == 'B')
		return 0;
	else
	{
		std::cout<<std::endl<<"err: the given register "<<reg<<" is not supported by architecture"<<std::endl;
		exit(0);
	}
}
////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////


///////////////////////////////////////////////// buffer builder for the file //////////////////////////////////////
typedef std::vector<char> buffer;
buffer readfile(char *filename)
{
	std::ifstream input(filename);
	if (input)
	{
		char ch='\0';
		buffer v;
		while(input.get(ch))
		{
			if( ch >= 32 && ch <= 126)
				v.push_back(ch);
		}
		return v;
	}
	else
		std::cout<<"err: No such file exist"<<std::endl;
	exit(1);
}
////////////////////////////////////////////////// bufer builder ends here /////////////////////////////////////////


///////////////////////////////////////////////// lexer startes here ///////////////////////////////////////////////
struct 	token
{
	TOKEN_CLASS tc;
	std::string value;
	int line_no;
};

bool is_legal_label(std::string lexem)
{
	if(lexem[0]=='_' || isalpha(lexem[0]))
	{
		for(int i=1; i < lexem.size() ; i++)
		{
			if(!(lexem[i]=='_' || isalpha(lexem[i]) || isdigit(lexem[i]) || lexem[i]=='-') )
				return false;
		}
		return true;
	}
	return false;
}


TOKEN_CLASS resolve_token_class(std::string lexem)
{
	for(int i = 0 ; i < ID0_LIST.size() ; i++)
	{
		if(lexem == ID0_LIST[i].name)
			return ID0;
	}
	for(int i = 0 ; i < ID1_LIST.size() ; i++)
	{
		if(lexem == ID1_LIST[i].name)
			return ID1;
	}

	if(lexem.size() == 1)
	{
		if(lexem[0] == 'A')
			return REGISTER;
		if(lexem[0] == 'B')
			return REGISTER;
		if(lexem[0] == 'C')
			return REGISTER;
		if(lexem[0] == 'D')
			return REGISTER;
		if(lexem[0] == 'E')
			return REGISTER;
		if(lexem[0] == 'H')
			return REGISTER;
		if(lexem[0] == 'L')
			return REGISTER;
		if(lexem[0] == 'M')
			return REGISTER;
	}

	if(is_legal_label(lexem))
	{
		return LABEL;
	}

	if(lexem.size() == 3)
	{
		if(isdigit(lexem[0]) && isdigit(lexem[1]) && (lexem[2] == 'H'))
			return DATA;
	}
	
	if(lexem.size() == 5)
	{
		if(isdigit(lexem[0]) && isdigit(lexem[1]) && isdigit(lexem[2]) && isdigit(lexem[3]) && (lexem[4] == 'H'))
			return ADDR;
	}

	

	return UKN;

}



/*=================UTILITY FOR LEXER ==================================================*/
void printTokens(std::vector<token> TOKENISED_SOURCE)
{
	for(int i=0 ; i < TOKENISED_SOURCE.size() ; i++)
	{
		std::cout<<"< "<<TOKENISED_SOURCE[i].tc<<" , "<<TOKENISED_SOURCE[i].value<<" , "<<TOKENISED_SOURCE[i].line_no<<" , "<<">"<<std::endl;
	}
}

void printToken(token t)
{
	std::cout<<"< "<<t.tc<<" , "<<t.value<<" , "<<t.line_no<<" , "<<">"<<std::endl;
}
/*======================================================================================*/

/*==========================LEX ANALYSER CREATING SYMBOL TABLE==========================*/
typedef std::vector<token> symbol_table;
symbol_table lex_analyse_source(buffer b)
{
	symbol_table TOKENISED_SOURCE;
	char ch=' ';
	std::string lexem="";
	int line_no=1;

	for(int i = 0; i < b.size() ; i++)
	{
		ch = b[i];
		if(ch == SPACE) // seppration of operation to operands
		{
			if(lexem == "") // if nothing is being scanned then why do anything anywaw
				continue;
			TOKEN_CLASS tc = resolve_token_class(lexem); 
			
			if(tc == UKN)
			{
				std::cout<<std::endl<<"err: unknown token "<<lexem<<" at line "<<line_no<<std::endl;
				exit(0);
			}

			token t = { tc ,lexem ,line_no };
			TOKENISED_SOURCE.push_back(t);
			lexem="";
			continue;
		}
		else if(ch == EOL) // separation of lines
		{
			if(lexem == "") // if nothing is being scanned then why do anything anywaw
				continue;
			TOKEN_CLASS tc = resolve_token_class(lexem); 
			
			if(tc == UKN)
			{
				std::cout<<std::endl<<"err: unknown token "<<lexem<<" at line "<<line_no<<std::endl;
				exit(0);
			}

			token t = { tc ,lexem ,line_no };
			TOKENISED_SOURCE.push_back(t);
			TOKENISED_SOURCE.push_back({EOL, ";" , line_no });
			line_no++;
			lexem="";
			continue;
		}
		else if( ch == COMMA) // seppration of arguements
		{
			if(lexem == "") // if nothing is being scanned then why do anything anywaw
				continue;
			TOKEN_CLASS tc = resolve_token_class(lexem); 
			
			if(tc == UKN)
			{
				std::cout<<std::endl<<"err: unknown token "<<lexem<<" at line "<<line_no<<std::endl;
				exit(0);
			}

			token t = { tc ,lexem ,line_no };
			TOKENISED_SOURCE.push_back(t);
			TOKENISED_SOURCE.push_back({COMMA , "," , line_no });
			lexem="";
			continue;	
		}
		else if( ch == COLON) // labels
		{
			if(lexem == "") // if nothing is being scanned then why do anything anywaw
				continue;
			TOKEN_CLASS tc = resolve_token_class(lexem); 
			
			if(tc == UKN)
			{
				std::cout<<std::endl<<"err: bad label "<<lexem<<" at line "<<line_no<<std::endl;
				exit(0);
			}

			token t = { tc ,lexem ,line_no };
			TOKENISED_SOURCE.push_back(t);
			TOKENISED_SOURCE.push_back({COLON , ":" , line_no });
			lexem="";
			continue;
		}
		lexem+=ch;
	}
	// printTokens(TOKENISED_SOURCE);
	return TOKENISED_SOURCE;
}
/////////////////////////////////////////////////// lexer ends here /////////////////////////////////////////////////////

/*=============================================PARSER FOR THE SYMBOL TABLE=============================================*/
struct LABEL_TABLE_ENTRY
{
	std::string LABEL_NAME;
	int mem_loc; 
	bool resolve; // true => the JMP statement will look for those whose resolve is true i.e needs resolving.
};
typedef std::vector<LABEL_TABLE_ENTRY> label_table; 
typedef std::vector<std::string> binarySource;
/*===========UTILITY STUFF FOR PRINTINTS AND STUFF*=========================*/

void print_binary_source(binarySource &li)
{
	std::cout<<std::endl;
	for(int i=0 ; i < li.size() ; i++)
	{
		std::cout<<i<<" : "<<li[i]<<std::endl;
	}
}

/*==========================================================================*/

// gets the code from the binary code tables based on which class of ID it is 
unsigned char getCode(std::string operation , TOKEN_CLASS tc)
{
	if(tc == ID0)
		for(int i=0 ; i < ID0_LIST.size() ; i++)
		{
			if(ID0_LIST[i].name == operation)
				return ID0_LIST[i].binary;
		}
	if(tc == ID1)
		for(int i=0 ; i < ID1_LIST.size() ; i++)
		{
			if(ID1_LIST[i].name == operation)
				return ID1_LIST[i].binary;
		}
	std::cout<<std::endl<<"err: cant find opecode for "<<operation<<std::endl;
	exit(0);
}



binarySource parse_symbol_table(symbol_table symt , std::string start_point)
{
	/*
		LABEL: COLON ID0 | COLON ID1
		ID0: ; | R,R; | R; 
		ID1: ADDR; | R,D; | D; | LABEL;
	*/

	
	label_table LABEL_TABLE;
	binarySource TRANSLATED_SOURCE;

	int state=0;
	int look_back=0;
	int mem_loc=0;

	for(int i = 0 ; i < symt.size() ; i++)
	{
		mem_loc = TRANSLATED_SOURCE.size(); 
		switch(state)
		{
			case 0 : {
				if(symt[i].tc == ID0 || symt[i].tc == ID1) // ID0 => S1
					state=1;
				else if(symt[i].tc == LABEL)
				{
					bool found=false; // is the label already existing in the label_table
					for(int k=0 ; k < LABEL_TABLE.size() ; k++) // iterate over the label table
					{
						if(LABEL_TABLE[k].LABEL_NAME == symt[i].value) // if the label is found
						{
							if(LABEL_TABLE[k].resolve == false) // was this label entered by a start synbol i.e by rule LABEL: ID0|ID1
							{
								std::cout<<"err: reuse of label "<<symt[i].value<<" for denoting jump position at line "<<symt[i].line_no<<std::endl;
								exit(1); // if yes then exit since the label is getting used
							}
							// else resolve the label location in the jump statement
							unsigned short sp= stoi(start_point , nullptr , 16); // converting 0x8000 to decimal integer
							std::string addr = std::bitset<2*BINARY_WORD_SIZE>(sp + mem_loc).to_string();
							// entering current location + start point in the the JMP location that created the label table entry
							TRANSLATED_SOURCE[LABEL_TABLE[k].mem_loc+1] = addr.substr(BINARY_WORD_SIZE,BINARY_WORD_SIZE);
							TRANSLATED_SOURCE[LABEL_TABLE[k].mem_loc+2] = addr.substr(0,BINARY_WORD_SIZE);
							// it is resolved hence delete the entry and decrement the counter
							// LABEL_TABLE.erase(LABEL_TABLE.begin()+k);
							// k--;
							found = true;
						}
					}
					// if not found then the following steps to be taken
					if(!found)
					{	// not found i.e new label entry to be created for resolving by JMP statement
						LABEL_TABLE.push_back({symt[i].value , mem_loc , false});
					}
					state=1;
				}
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}
				break;
			}

			case 1 : {
				if(symt[i].tc == EOL) // ID0 SEMICOLON
				{
					look_back = i-1; // number_of_state passed
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(getCode(symt[look_back].value , symt[look_back].tc)).to_string());
					
					state=0;
				}
				else if(symt[i].tc == REGISTER) // ID0 REGISTER => S2
					state=2;
				else if(symt[i].tc == ADDR) // ID1 ADDR => S3
					state=3;
				else if(symt[i].tc == DATA) // ID1 DATA => S6
					state=6;
				else if(symt[i].tc == LABEL) // ID1 LABEL => S7
					state =7;
				else if(symt[i].tc == COLON) // LABEL COLON => RESET
					state = 0;
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}
				break;
			}
			case 2 : {
				if(symt[i].tc == EOL) // ID0 REGISTER SEMICOLON => RESET
				{
					look_back = i-2; // number of state passed
					unsigned char oppcode = (getCode(symt[look_back].value , symt[look_back].tc));
					char reg = symt[look_back+1].value[0];
					if(oppcode == ADD_MAP) // for ADD operation
					 	oppcode = ADD_TABLE[get_register_pos(reg)];
					else
					{
						std::cout<<std::endl<<"err: the given operation "<<symt[look_back].value<<"at line "<<symt[look_back].line_no<<" with one register operand is part of architecture but does not have a table for hex code"<<std::endl;
						exit(0);
					}
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					 // to be added more
					
					state = 0;
				}
				else if(symt[i].tc == COMMA) // IDO REGISTER COMMA => S3
					state = 3;
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}

				break;
			}

			case 3 :{
				if(symt[i].tc == REGISTER) // ID0 REGISTER COMMA REGISTER => S4
					state=4;
				else if(symt[i].tc == EOL) // ID1 ADDR SEMICOLOR => RESET
				{
					look_back = i - 2; // two states back
					unsigned char oppcode = (getCode(symt[look_back].value , symt[look_back].tc));
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					look_back++;
					 // this is a problem i have no ide why it is there
					// if we put statement ot t in directly in stoi then it gives out of bounds exception but why ?????
					// OPEN BUG 
					std::string lower_bit = symt[look_back].value.substr(2,2);
					oppcode = std::stoi(lower_bit, nullptr , 16); // lower bit address arguements
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					oppcode = std::stoi(symt[look_back].value.substr(0,2), nullptr , 16);// higher bit address arguements
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					state=0;

				}
				else if(symt[i].tc == DATA) // ID1 REGISTER COMMA DATA => S5
					state = 5;
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}
				break;
			}

			case 4 :{
				if(symt[i].tc == EOL) // ID0 REGISTER COMMA REGISTER SEMICOLON => RESET
				{
					look_back = i-4; // number of state passed
					unsigned char oppcode = (getCode(symt[look_back].value , symt[look_back].tc));
					char reg1 = symt[look_back+1].value[0];
					char reg2 = symt[look_back+3].value[0]; // skipping the COMMA in between
					if(oppcode == MOV_MAP) // for MOV opperation
					{
						oppcode = MOV_TABLE[get_register_pos(reg1)][get_register_pos(reg2)];
					}
					else
					{
						std::cout<<std::endl<<"err: the given operation "<<symt[look_back].value<<" at line "<<symt[look_back].line_no<<" with two register operands is not part of architecture or does not have its opcode table specified"<<std::endl;
						exit(0);
					}

					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					 // to be added more
					state = 0;
					
				}
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}
				break;
			}

			case 5 :{
				if(symt[i].tc == EOL) // ID1 REGISTER COMMA DATA SEMICOLON => RESET
				{
					look_back = i-4; // number of state passed
					unsigned char oppcode = (getCode(symt[look_back].value , symt[look_back].tc));
					char reg1 = symt[look_back+1].value[0];
					if( oppcode == MVI_MAP)
					{
						oppcode = MVI_TABLE[get_register_pos(reg1)];

					}
					else
					{
						std::cout<<std::endl<<"err: the given operation "<<symt[look_back].value<<" at line "<<symt[look_back].line_no<<" with one register and data operand is not part of architecture or does not have its opcode table specified"<<std::endl;
						exit(0);
					}
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					oppcode = std::stoi(symt[look_back+3].value.substr(0,2), nullptr , 16);
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					
					state=0;
				}
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}
				break;
			}

			case 6 :{
				if(symt[i].tc == EOL) // ID1 DATA SEMICOLON => RESET
				{
					look_back = i-2; // number of state passed
					unsigned char oppcode = (getCode(symt[look_back].value , symt[look_back].tc));
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					oppcode = std::stoi(symt[look_back+1].value.substr(0,2), nullptr , 16);
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					
					state=0;
				}
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}
				break;
			}

			case 7 :{
				if(symt[i].tc == EOL) // ID1 LABEL SEMICOLON => RESET
				{
					look_back = i-2; // number of state passed
					unsigned char oppcode = (getCode(symt[look_back].value , symt[look_back].tc));
					TRANSLATED_SOURCE.push_back(std::bitset<BINARY_WORD_SIZE>(oppcode).to_string());
					bool found=false;
					// see whether the label exist in the list already or not
					for(int k=0 ; k < LABEL_TABLE.size() ; k++)
					{
						if(LABEL_TABLE[k].resolve==false && LABEL_TABLE[k].LABEL_NAME == symt[look_back+1].value)
						{
							unsigned short sp= stoi(start_point , nullptr , 16); // converting 0x8000 to decimal integer
							std::string addr = std::bitset<2*BINARY_WORD_SIZE>(sp + LABEL_TABLE[k].mem_loc).to_string();
							TRANSLATED_SOURCE.push_back(addr.substr(BINARY_WORD_SIZE,BINARY_WORD_SIZE));
							TRANSLATED_SOURCE.push_back(addr.substr(0,BINARY_WORD_SIZE));
							found = true;
							break;
						}
					}
					if(!found)
					{
						TRANSLATED_SOURCE.push_back("");
						TRANSLATED_SOURCE.push_back("");
						LABEL_TABLE.push_back({symt[look_back+1].value , mem_loc , true});
					}
					state=0;
				}
				else
				{
					std::cout<<std::endl<<"err: cannot parse the line "<<symt[i].line_no<<std::endl;
					exit(0);
				}
				break;
			}

		}
	}


	for(int k=0 ; k < TRANSLATED_SOURCE.size() ; k++)
	{
		if(TRANSLATED_SOURCE[k] == "")
		{
			std::cout<<"err: possible unresolved labels";
			exit(1);
		}		
	}

	return TRANSLATED_SOURCE;
}

/////////////////////////////////////////////////////// parser ends ////////////////////////////////////////////////////

////////////////////////////////////////////////////// WRITE FILE   //////////////////////////////////////////////////////

void writeFile(binarySource bs, std::string filename)
{
	std::ofstream output(filename);
	if (output)
	{
		for(int i = 0 ; i < bs.size() ; i++)
		{
			output<<bs[i]<<"\n";
		}
		output.close();
	}
	else
		std::cout<<"err: failed to create file "<<std::endl;
	exit(1);
}




int main(int argc  , char *argv[])
{
	if(argc < 2)
		std::cout<<"err: No file given"<<std::endl;
	else
		{
			std::string start_point="8000";
			if(argc == 3)
			{
				start_point = argv[2];
			}	
			auto b = readfile(argv[1]);
			auto st = lex_analyse_source(b);
			auto ts = parse_symbol_table(st , start_point);
			writeFile(ts , "a.dat");
		}
}