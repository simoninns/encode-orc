# Specification references

These requirements are according to IEC 60856-1986 Laservision PAL and IEC 60857-1986 Laservision NTSC.

# Vertical Interval Control and Address Signals for PAL

The code signals on the video disk provide special information which can be utilized by the player to control special functions and to provide picture frame or time information.

The CAV format has the following types of codes:

1. Lead-in  
2. Lead-out  
3. Picture numbers  
4. Picture stop  
5. Chapter numbers  
6. Programme status code  
7. Users code  

On CLV format the codes are:

1. Lead-in  
2. Lead-out  
3. Programme time code  
4. CLV code  
5. Chapter numbers  
6. CLV picture number  
7. Programme status code  
8. Users code  

## 24-bit Biphase Coded Signal

This signal is inserted in selected video lines during the vertical interval. It is subdivided into 6 groups of 4 bits and each group can be any hexadecimal word. The first group of 4 bits is the key and starts with a logic one. Each bit cell is 2µs long with the digital level between 30% and 100% (0 to 100 IRE for NTSC).

### Lead-in

The lead-in code indicates the start of the programme. The 24-bit biphase lead-in code with a hexadecimal value of "88FFFF" is inserted into lines 17, 18, 330 and 331 (17, 18, 280 and 281 for NTSC) during at least a number of tracks corresponding to 1.5 mm prior to the active programme start.

### Lead-out

The lead-out code indicates the end of the programme. The 24-bit biphase lead-out code with a hexadecimal value of "80EEEE" is inserted in lines 17, 18, 330 and 331 (17, 18, 280 and 281 for NTSC) during at least 2mm after the end of the active programme.

### Picture Numbers

The picture numbers shall be present during the active programme on CAV disks. They are unique and in a normal count sequence starting with number 1 at the beginning of the active programme.

The picture numbers shall be inserted into lines 17 and 18 or in lines 330 and 331, (17 and 18 or into lines 280 and 281 for NTSC) depending on which field is the first of the picture. The hexadecimal value is: FX<sub>1</sub>X<sub>2</sub>X<sub>3</sub>X<sub>4</sub>X<sub>5</sub>  where X<sub>1</sub> through X<sub>5</sub> represent the picture number, X<sub>5</sub> being the least significant digit. The maximum available picture number is 99999 (79999 for NTSC).

### Picture Stop Code

On CAV disks, the picture stop code enables the playback equipment to switch automatically to the still picture mode from normal speed or slow motion. The 24-bit biphase picture stop code with a hexadecimal value of "82CFFF" is inserted in lines 16 and 17 or 329 and 330 (16 and 17 or 279 and 280 for NTSC) of the field immediately following the field in which the 24-bit picture number was inserted to enable stopping on the selected picture. On CLV disks there is no picture stop code.

### Chapter Numbers

Chapter numbers indicate parts of the programme as a chapter and are optional. They are unique and in a normal count sequence starting with a selectable number at the beginning of the active programme (i.e. number "0" or "1" or a pre-set number consecutive to the last number of a previous disk with the same programme content). The 24-bit biphase coded chapter numbers, if present, are inserted in lines 17, 18 and 330, 331 (17, 18 and 280, 281 for NTSC) in the fields of the whole active programme area which do not have an insertion of picture numbers on CAV disks. However, for lines 17 and 330 (17 and 280 for NTSC), picture stop code has priority.

On CLV disks, they are inserted in line 18 or 331 (18 or 281 for NTSC) in the fields of the whole active programme area which do not have an insertion of programme time code and CLV picture number.

Each chapter number starts with a stop-bit (the first bit after the key) at a zero-logic value during 400 tracks followed by at least 400 tracks with a stop-bit at a one-logic value until the next chapter starts. The zero value stop-bit is intended to disable the search action of the player. The first chapter directly after the lead-in area shall not have a stop-bit of zero-logic value. The hexadecimal value is "8X<sub>1</sub>X<sub>2</sub>DDD". X<sub>1</sub> and X<sub>2</sub> are the chapter numbers. The maximum number is 79.

On disks with chapters shorter than 800 tracks the stop bit of each chapter number shall have the logic value "one". The minimum length of a chapter will be 30 tracks.

###  Programme Time Code

The programme time code is always present on-CLV disks during the active programme and indicates the running time (expressed in hours and minutes). The 24-bit biphase programme time code with a hexadecimal value of "FX<sub>1</sub> DDX<sub>2</sub>X<sub>3</sub> " is inserted in lines 17 and 18 or 330 and 331 (17 and 18 or 280 and 281 for NTSC) depending on which field is the first field of the picture.

X<sub>1</sub> indicates the hours
X<sub>2</sub> and X<sub>3</sub> indicate the minutes

### Constant Linear Velocity (CLV) Code

The CLV code is always present in the active programme on a CLV disk.

It indicates the CLV format. The 24-bit CLV code with a hexadecimal value of "87FFFF" is inserted in line 330 or 17 (280 or 17 for NTSC) in the fields of the whole active programme area which do not have an insertion of programme time code and CLV picture number.

### Programme Status Code

The programme status code identifies the use of the audio and video channels and will be inserted in the active programme area.

Code: 8 DC/BA X3, X4, X5 (see "Definition of the data in programme status code" below for explanation)

Insertion on CLV disks is: line 329 or 16 (279 or 16 for NTSC), in the same fields where CLV code is inserted.

On CAV disks the insertion is: lines 16 and 329 (16 and 279 for NTSC).

Note: Picture stop code has priority over the programme status code.

#### Definition of the data in programme status code `8 DC/BA X3, X4, X5`

**DC**  CX noise reduction on  
**BA**  CX noise reduction off  

**X31** indicates disk size:  
- `0` = 12-inch  
- `1` = 8-inch  

**X32** indicates disk side:  
- `0` = first side  
- `1` = second side  

**X33** indicates if there are teletext signals present anywhere on the disk or not:  
- `0` = teletext signals absent  
- `1` = teletext signals present  

**X34** indicates if the audio signal is FM–FM multiplex modulated or not:  
- `0` = FM–FM multiplex off  
- `1` = FM–FM multiplex on  

**X42** indicates if the video format contains normal analogue video signal or, during the  
active parts of the line, a digital signal:  

- `0` = analogue video  
- `1` = digital signal  

**Note.** — This indication of these digital signals in the video is not mandatory but can be an option for the programme maker.

**X41, X43, X44** together with **X34** indicate the status of the audio channels according to the following table:

| X41 | X34 | X43 | X44 | Programme dump | FM–FM multiplex | Channel I | Channel II |
|-----|-----|-----|-----|----------------|-----------------|-----------|------------|
| 0 | 0 | 0 | 0 | off | off | stereo | stereo |
| 0 | 0 | 0 | 1 | off | off | mono | mono |
| 0 | 0 | 1 | 0 | off | off | future use | future use |
| 0 | 0 | 1 | 1 | off | off | bilingual | bilingual |
| 0 | 1 | 0 | 0 | off | on | stereo | stereo |
| 0 | 1 | 0 | 1 | off | on | stereo | bilingual |
| 0 | 1 | 1 | 0 | off | on | cross channel stereo | cross channel stereo |
| 0 | 1 | 1 | 1 | off | on | bilingual | bilingual |
| 1 | 0 | 0 | 0 | on | off | mono dump | mono dump |
| 1 | 0 | 0 | 1 | on | off | mono dump | mono dump |
| 1 | 0 | 1 | 0 | on | off | future use | future use |
| 1 | 0 | 1 | 1 | on | off | mono dump | mono dump |
| 1 | 1 | 0 | 0 | on | on | stereo dump | stereo dump |
| 1 | 1 | 0 | 1 | on | on | stereo dump | stereo dump |
| 1 | 1 | 1 | 0 | on | on | bilingual dump | bilingual dump |
| 1 | 1 | 1 | 1 | on | on | bilingual dump | bilingual dump |

**Note.** — The indication of programme dump (**X41**) is not mandatory, but an option for the  
programme maker.

**X5** is an error check code on **X4** with even parity bit, according to **Hamming Code**.

**X51** is the parity with **X41, X42 and X44**  

**X52** is the parity with **X41, X43 and X44**  

**X53** is the parity with **X42, X43 and X44**

#### Hamming Code

— Information vector X<sub>4</sub>:  A = [a<sub>1</sub>, a<sub>2</sub>, a<sub>3</sub>, a<sub>4</sub>]  
— Check vector X<sub>5</sub>: C = [c<sub>1</sub>, c<sub>2</sub>, c<sub>3</sub>]  

with parity bit: 
```
           4        3
     c4 =  Σ ai  +  Σ cj  (modulus 2)
          i = 1     j = 1
```

— Encoding   V = A · G = [a<sub>1</sub>, a<sub>2</sub>, a<sub>3</sub>, a<sub>4</sub>, c<sub>1</sub>, c<sub>2</sub>, c<sub>3</sub>]

    Where G is the matrix:

    ⎡1 0 0 0 1 1 0⎤
    ⎢0 1 0 0 1 0 1⎥
    ⎢0 0 1 0 0 1 1⎥
    ⎣0 0 0 1 1 1 1⎦

— Read out code U = [a<sub>1</sub>, a<sub>2</sub>, a<sub>3</sub>, a<sub>4</sub>, c<sub>1</sub>, c<sub>2</sub>, c<sub>3</sub>] 

— Decoding: Syndrome:  S = U · M = [s<sub>1</sub>, s<sub>2</sub>, s<sub>3</sub>]

Where M is the matrix:

    ⎡1 1 0⎤
    ⎢1 0 1⎥
    ⎢0 1 1⎥
    ⎢1 1 1⎥
    ⎢1 0 0⎥
    ⎢0 1 0⎥
    ⎣0 0 1⎦


| s<sub>1</sub> | s<sub>2</sub> | s<sub>3</sub> | Correction if 1 bit error |
|---:|---:|---:|---------------------------|
| 0  | 0  | 0  | No error                  |
| 1  | 0  | 0  | c<sub>1</sub>                        |
| 0  | 1  | 0  | c<sub>2</sub>                        |
| 1  | 1  | 0  | a<sub>1</sub>                        |
| 0  | 0  | 1  | c<sub>3</sub>                        |
| 1  | 0  | 1  | a<sub>2</sub>                        |
| 0  | 1  | 1  | a<sub>3</sub>                        |
| 1  | 1  | 1  | a<sub>4</sub>                        |

— Error detection with parity (c<sub>4</sub>):

    1. If S = 0 then U is valid  
    2. If S ≠ 0 and parity is error, then U can be corrected from S  
    3. If S ≠ 0 but parity is valid, then U includes a two bit error not to be correc

### Users Code

The users code is intended for filing and identification and can be inserted as an option in the lead-in and lead-out area. The data content is up to the disk manufacturer.

Code: 8 X1 D X3 X4 X5

X1 = 0 through 7; X3, X4, X5 = 0 through F.

Insertion in lead-in and/or lead-out area in the lines 16, 329 (16, 279 for NTSC).

### CLV Picture Number

On the CLV disk the CLV picture number identifies each video frame and can also be used to detect hang-ups.

Code: 8 Xl E X3 X4 X5

X1 = A through F: X3 = 0 through 9

X1 and X3 indicate the seconds of the run time together with the hours and minutes of the programme time code.

X4 and X5 are the picture numbers within 1s, thus:

X4 = 0 through 2 and X5 = 0 through 9.

The CLV picture number shall be inserted into line 16 or 329 (16 or 279 for NTSC) depending on which field is the first field of the picture.

# Amendment 2

Both the PAL and NTSC IEC specification have additional requirements in the "amendment 2" documents.

TBC
