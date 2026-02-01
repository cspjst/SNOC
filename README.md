# SNOC Library
## Implementing the StriNg Oriented symBOlic Language patterns in C.

Importing the practical discrete recognition systems from SNOBOL4 into C for powerful string handling and parsing whilst maintaing C idioms. 

SNOBOL4 patterns are more powerful than regular expressions (Chomsky Type-3) and can express context-free grammars (Chomsky Type-2).

### Definitions
+ The _subject string_ is the string currently being scanned; it is the entire context of the pattern matching operation. 
+ The number n in POS(n) and TAB(n) is with reference to the beginning of the subject string. 
+ The number n in RTAB(n) and RPOS(n) is with referenceto the right end of the subject string. 
+ The _scanner_ is the program that attempts to match the pattern against the subject string. 
+ The cursor is a pointer which points into the subject string; it can take on the values 0,1 , . . . , k where k is the length of the subject.
