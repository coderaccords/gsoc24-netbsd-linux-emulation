#define TEOF 0
#define TNL 1
#define TSEMI 2
#define TBACKGND 3
#define TAND 4
#define TOR 5
#define TPIPE 6
#define TLP 7
#define TRP 8
#define TENDCASE 9
#define TENDBQUOTE 10
#define TREDIR 11
#define TWORD 12
#define TIF 13
#define TTHEN 14
#define TELSE 15
#define TELIF 16
#define TFI 17
#define TWHILE 18
#define TUNTIL 19
#define TFOR 20
#define TDO 21
#define TDONE 22
#define TBEGIN 23
#define TEND 24
#define TCASE 25
#define TESAC 26
#define TNOT 27

/* Array indicating which tokens mark the end of a list */
const char tokendlist[] = {
	1,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	0,
	0,
	0,
	1,
	1,
	0,
	1,
	0,
	1,
	0,
};

const char *const tokname[] = {
	"end of file",
	"newline",
	"\";\"",
	"\"&\"",
	"\"&&\"",
	"\"||\"",
	"\"|\"",
	"\"(\"",
	"\")\"",
	"\";;\"",
	"\"`\"",
	"redirection",
	"word",
	"\"if\"",
	"\"then\"",
	"\"else\"",
	"\"elif\"",
	"\"fi\"",
	"\"while\"",
	"\"until\"",
	"\"for\"",
	"\"do\"",
	"\"done\"",
	"\"{\"",
	"\"}\"",
	"\"case\"",
	"\"esac\"",
	"\"!\"",
};

#define KWDOFFSET 13

const char *const parsekwd[] = {
	"if",
	"then",
	"else",
	"elif",
	"fi",
	"while",
	"until",
	"for",
	"do",
	"done",
	"{",
	"}",
	"case",
	"esac",
	"!",
	0
};
