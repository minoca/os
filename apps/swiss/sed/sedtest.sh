##
## Sed test script
## Evan Green
## 12-Jul-2013
##

##
## Define the correct output of the sed program.
##

SED_OUTPUT='hiya
my
wonderful
amigo

really

great
7
9
up
up\nin$
up
in
old 
up
in
town
where
the\nup\nin\ntown$
the
up
in
town
15
old
cactus
cactus
grow
grow
hiya
there
friend
or enemy?

great
18
up
in
town
and
nobody
knows
what 
    yoooou   
GONE
done
##26
abcdefghbcdefgh&xxcdecdeh***zzijklmno
abCfghbCfgh&CCh***ZZijklmno
My
Great
Day
12345678901234Fifteen67890
12345678901234Fifteen67890'

##
## Define the correct outputs for wfile1 and wfile2
##

SED_WFILE1='the
up
in
town
'

SED_WFILE2='12345678901234Fifteen67890**zzijklmno
abCfghbCfgh&xxCCh***ZZijklmno
'

##
## Define the input script
##

SED_SCRIPT='
b BeginLabel
athis\
never\
runs
:QuitLabel
q
:BeginLabel
2 {
  a\
my
  awonderful
}
2,3	camigo
\cgreatc    i\
really\n
/ enemy?/ d
/ enemy?/,$ d
7 {=;N;N;D;p;}
9 =;9P;9l
9h;;;
11g
12H;14G
14l
14w wfile1
15{=    ;;;  n;15q}
16p
17p
17r stesta
18=;18x
\ah\avea y/ahve/OGNE/
\ah\avea =
27 {s/b\(c..\)fg\(h\)/&&\&xx\1\1\2***zz/pw wfile2
	s_c.._C_g
	tOverLabel
	q

	:OverLabel
	swzwZwgw wfile2
	s/x//g
	t
	tQuitLabel
	s/QQQ/DOESNOTMATCH/g
	t   QuitLabel
}
28,/cutstop/d
29p
32s/NEWLINE/\n/g
33s.\..Fifteen.15pw ././wfile2
99tQuitLabel



##
## These are comments
#nawef
'

##
## Define the contents of the input files.
##

SED_INPUT1='hiya
there
friend
or enemy?

great
'

SED_INPUT2='whats
up
in
old 
sasprilluh
town
where
the
old
cactus
grow

and
nobody
knows
what 
    yoooou   
have
done
##26
abcdefghijklmno
cutstart
does
not appear
cutstop
MyNEWLINEGreatNEWLINEDay
12345678901234567890'

##
## Get set up to run the test by outputting the input 
## files and script.
##

echo -n "$SED_INPUT1" > stesta
echo -n "$SED_INPUT2" > stestb
echo -n "$SED_SCRIPT" > stest

##
## Run the test.
##

sed -f stest stesta stestb > stestout
SED_RETURN=$?

echo -n "$SED_OUTPUT" > stestout.ans
echo -n "$SED_WFILE1" > wfile1.ans
echo -n "$SED_WFILE2" > wfile2.ans

##
## Compare the results.
##

SED_RECEIVED_OUTPUT="`cat stestout`"
SED_RECEIVED_WFILE1="`cat wfile1`"
SED_RECEIVED_WFILE2="`cat wfile2`"
SED_ERRORS=0
if [ $SED_RETURN -ne 0 ]; then
	echo "sed returned failing status $SED_RETURN."
	SED_ERRORS=$(($SED_ERRORS+1))
fi

if ! cmp stestout stestout.ans; then
	echo "sed output failed. Compare stestout with stestout.ans"
	SED_ERRORS=$(($SED_ERRORS+1))
fi

if ! cmp wfile1 wfile1.ans; then
	echo "sed wfile1 failed. Compare wfile1 with wfile1.ans"
	SED_ERRORS=$(($SED_ERRORS+1))
fi

if ! cmp wfile2 wfile2.ans; then
	echo "sed wfile2 failed. Compare wfile2 with wfile2.ans"
	SED_ERRORS=$(($SED_ERRORS+1))
fi

if [ $SED_ERRORS -ne 0 ]; then
	echo "*** sed test had $SED_ERRORS failures ***"
fi

if [ $SED_ERRORS -eq 0 ]; then
	rm stestout stestout.ans wfile1 wfile1.ans wfile2 wfile2.ans
	rm stesta stestb stest
fi
