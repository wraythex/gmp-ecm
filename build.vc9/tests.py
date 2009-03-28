
from __future__ import print_function
import os
import sys
import string
import platform
from re import match
from subprocess import Popen, PIPE, STDOUT
from tempfile import *

test_dir = ".\\x64\\Release-AMD\\"
# test_dir = ".\\x64\\Release-Intel\\"
# test_dir = ".\\win32\\Release-AMD\\"
# test_dir = ".\\win32\\Release-Intel\\"

ecm = [
  ("2050449353925555290706354283", "-sigma 7 -k 1 30 0-1e6", 14),
  ("137703491", "-sigma 6 84 1000", 8),
  ("3533000986701102061387017352606588294716061", "-sigma 1621 191 225", 14),
  ("145152979917007299777325725119", "-sigma 711387948 924 117751", 14),
  ("2^919-1", "-sigma 262763035 937 1", 6),
  ("2^919-1", "-sigma 1691973485 283 1709", 6),
  ("(2^1033+1)/3", "-sigma 2301432245 521 1", 6),
  ("(2^1033+1)/3", "-sigma 2301432245 223 1847", 6),
  ("(2^1063+1)/3/26210488518118323164267329859", "-sigma 2399424618 383 1", 6),
  ("(2^1063+1)/3/26210488518118323164267329859", "-sigma 2399424618 71 500", 6),
  ("242668358425701966181147598421249782519178289604307455138484425562807899", "-sigma 1417477358 28560 8e7-85507063", 14),
  ("3533000986701102061387017352606588294716061", "-sigma 291310394389387 191 225", 14),
  ("121279606270805899614487548491773862357", "-sigma 1931630101 120", 14),
  ("291310394389387", "-power 3 -sigma 40 2000", 8),
  ("3533000986701102061387017352606588294716061", "-sigma 3547 167 211", 14),
  ("449590253344339769860648131841615148645295989319968106906219761704350259884936939123964073775456979170209297434164627098624602597663490109944575251386017", "-sigma 63844855 -go 172969 61843 20658299", 14),
  ("17061648125571273329563156588435816942778260706938821014533", "-sigma 585928442 174000", 14),
  ("89101594496537524661600025466303491594098940711325290746374420963129505171895306244425914080753573576861992127359576789001", "-sigma 877655087 -go 325001 157721 1032299", 14),
  ("5394204444759808120647321820789847518754252780933425517607611172590240019087317088600360602042567541009369753816111824690753627535877960715703346991252857", "-sigma 805816989 -go 345551 149827", 6),
  ("3923385745693995079670229419275984584311007321932374190635656246740175165573932140787529348954892963218868359081838772941945556717", "-sigma 876329474 141667 150814537", 14),
  ("124539923134619429718018353168641490719788526741873602224103589351798060075728544650990190016536810151633233676972068237330360238752628542584228856301923448951", "-sigma 1604840403 -go 983591971839332299 96097 24289207", 14),
  ("5735013127104523546495917836490637235369", "-power 60 -k 2 -A 3848610099745584498259560038340842096471 -x0 2527419713481530878734189429997880136878 330000 500000000", 8),
  ("17833653493084084667826559287841287911473", "-power 6 -k 2 -A 7423036368129288563912180723909655170075 -x0 9011819881065862648414808987718432766274 389797 16e8", 8),
  ("212252637915375215854013140804296246361", "-power 15 -k 2 -sigma 781683988 1000000", 8),
  ("4983070578699621345648758795946786489699447158923341167929707152021191319057138908604417894224244096909460401007237133698775496719078793168004317119431646035122982915288481052088094940158965731422616671", "-sigma 909010734 122861 176711", 6),
  ("1408323592065265621229603282020508687", "-sigma 1549542516 -go 2169539 531571 29973883000-29973884000", 8),
  ("3213162276640339413566047915418064969550383692549981333701", "-sigma 2735675386 -go 1615843 408997 33631583", 8),
  ("39614081257132168796771975177", "-sigma 480 1e6", 8),
  ("10000286586958753753", "-sigma 3956738175 1e6", 8),
  ("49672383630046506169472128421", "-sigma 2687434659 166669 86778487", 8),
  ("216259730493575791390589173296092767511", "-sigma 214659179 1124423 20477641", 8),
  ("49367108402201032092269771894422156977426293789852367266303146912244441959559870316184237", "-sigma 6 5000", 0),
  ("(2^1063+1)/3/26210488518118323164267329859", "-sigma 2399424618 383 1", 6),
  ("10090030271*10^400+696212088699", "-sigma 3923937547 1e3 1e6", 14)
]

pm1 = [
  ("441995541378330835457", "-pm1 -x0 3 157080 7e9-72e8", 8 ),
  ("335203548019575991076297", "-pm1 -x0 2 23 31", 8 ),
  ("335203548019575991076297", "-pm1 -x0 3 31 58766400424189339249-58766400424189339249", 8 ),
  ("2050449353925555290706354283", "-pm1 -k 1 20 0-1e6", 14 ),
  ("67872792749091946529", "-pm1 -x0 3 8467 11004397", 8 ),
  ("5735039483399104015346944564789", "-pm1 1277209 9247741", 8 ),
  ("620224739362954187513", "-pm1 -x0 3 668093 65087177", 8 ),
  ("1405929742229533753", "-pm1 1123483 75240667", 8 ),
  ("16811052664235873", "-pm1 -x0 3 19110 178253039", 8 ),
  ("9110965748024759967611", "-pm1 1193119 316014211", 8 ),
  ("563796628294674772855559264041716715663", "-pm1 4031563 14334623", 8 ),
  ("188879386195169498836498369376071664143", "-pm1 3026227 99836987", 8 ),
  ("474476178924594486566271953891", "-pm1 9594209 519569569", 8 ),
  ("2124306045220073929294177", "-pm1 290021 1193749003", 8 ),
  ("504403158265489337", "-pm1 -k 4 8 9007199254740700-9007199254740900", 8 ),
  ("6857", "-pm1 840 857", 8 ),
  ("10090030271*10^400+696212088699", "-pm1 2e3 2e6", 14)
# Try saving and resuming
# ("25591172394760497166702530699464321", "-pm1 -save test.pm1.save 100000
# checkcode $? 0
# $PM1 -resume test.pm1.save 120557 2007301
# C=$?
# /bin/rm -f test.pm1.save
# checkcode $C 8 ),
]

pp1 = [
  ("574535754974673735383001137423881", "-pp1 -x0 5 11046559 34059214979", 8 ),
  ("1212493270942550395500491620526329", "-pp1 -x0 9 1322743 15132776749", 8 ),
  ("12949162694219360835802307", "-pp1 -x0 5 3090877 362336209", 8 ),
  ("2224933405617843870480157177909", "-pp1 -x0 6 568751 573379", 8 ),
  ("6588443517876550825940165572081", "-pp1 -x0 5 308141 4213589", 8 ),
  ("951513164333845779921357796547797", "-pp1 -x0 5 991961 1927816573", 8 ),
  ("30273798812158206865862514296968537", "-pp1 -x0 5 24039443 5071284641", 8 ),
  ("4745647757936790297247194404494391", "-pp1 -x0 9 34652707 4267610467", 8 ),
  ("1267992248510159742851354500921987", "-pp1 -x0 5 205435127 3011959669", 8 ),
  ("3376019969685846629149599470807382641", "-pp1 -x0 5 16221563 125604601", 8 ),
  ("14783171388883747638481280920502006539", "-pp1 -x0 5 5963933 549138481", 8 ),
  ("884764954216571039925598516362554326397028807829", "-pp1 -x0 6 80105797 2080952771", 8 ),
  ("5703989257175782343045829011448227", "-pp1 -x0 6 2737661 581697661", 8 ),
  ("36542278409946587188439197532609203387", "-pp1 -x0 5 75484441 721860287", 8 ),
  ("23737785720181567451870298309457943", "-pp1 -x0 7 138563 9639649", 8 ),
  ("9535226150337134522266549694936148673", "-pp1 -x0 7 3037709 84506953", 8 ),
  ("68095768294557635629913837615365499", "-pp1 -x0 5 36936017 167452427", 8 ),
  ("3180944478436233980230464769757467081", "-pp1 -x0 5 7373719 764097571", 8 ),
  ("2879563791172315088654652145680902993", "-pp1 -x0 7 29850409 34290301", 8 ),
  ("79382035150980920346405340690307261392830949801", "-pp1 -x0 5 12073627 32945877451", 8 ),
  ("514102379852404115560097604967948090456409", "-pp1 -x0 8 223061 61500567937", 8 ),
  ("173357946863134423299822098041421951472072119", "-pp1 -x0 5 992599901 1401995848117", 8 ),
  ("183707757246801094558768264908628886377124291177", "-pp1 -x0 5 382807709 1052258680511", 8 ),
  ("16795982678646459679787538694991838379", "-pp1 -x0 6 2957579 26509499", 8 ),
# ("7986478866035822988220162978874631335274957495008401", "-pp1 -x0 17 1632221953 843497917739, 8),
# ("725516237739635905037132916171116034279215026146021770250523", "-pp1 -x0 5 51245344783 483576618980159", 8 ),
  ("1809864641442542950172698003347770061601055783363", "-pp1 -x0 6 21480101 12037458077389", 8 ),
  ("435326731374486648601801668751442584963", "-pp1 -x0 6 12002513 27231121", 8 ),
  ("3960666914072777038869829205072430197479", "-pp1 -x0 5 16534249 21802223243", 8)
]

pp1_2 = [
  ("328006342451", "-pp1 -x0 5 120 7043", 8 ),
  ("328006342451", "-pp1 -x0 1/5 120 7043", 8 ),
  ("2050449218179969792522461197", "-pp1 -x0 6 -k 1 20 0-1e6", 14),
  ("6215074747201", "-pp1 -power 2 -x0 5 630 199729", 8 ),
  ("6215074747201", "-pp1 -dickson 3 -x0 5 630 199729", 8 ),
  ("8857714771093", "-pp1 -x0 3 23251 49207", 8 ),
  ("236344687097", "-pp1 -x0 3 619 55001", 8 ),
  ("87251820842149", "-pp1 -x0 5 3691 170249", 8 ),
  ("719571227339189", "-pp1 -x0 4 41039 57679", 8 ),
  ("5468575720021", "-pp1 -x0 6 1439 175759", 8 ),
  ("49804972211", "-pp1 -x0 5 15443 268757", 8 ),
  ("329573417220613", "-pp1 -x0 3 5279 101573", 8 ),
  ("4866979762781", "-pp1 -x0 4 7309 97609", 8 ),
  ("187333846633", "-pp1 -x0 3 2063 9851", 8 ),
  ("332526664667473", "-pp1 -x0 3 65993 111919", 8 ),
  ("265043186297", "-pp1 -x0 3 8761 152791", 8 ),
  ("207734163253", "-pp1 -x0 3 1877 4211", 8 ),
  ("225974065503889", "-pp1 -x0 5 -k 5 7867 8243", 8 ),
  ("660198074631409", "-pp1 -x0 5 22541 115679", 8 ),
  ("563215815517", "-pp1 -x0 3 3469 109849", 8 ),
  ("563215815517", "-pp1 -x0 3 3469 109849-109849", 8 ),
  ("409100738617", "-pp1 -x0 3 19 19", 8 ),
  ("2277189375098448170118558775447117254551111605543304035536750762506158547102293199086726265869065639109", "-pp1 -x0 3 2337233 132554351", 14),
  ("630503947831861669", "-pp1 -x0 5 7 9007199254740000-9007199254741000", 8 ),
  ("8589934621", "-pp1 -x0 10 4294967310-4294967311 1", (1, 8) ),
  ("6054018161*10^400+417727253109", "-pp1 -x0 4 2e3 2e6", 14),
  ("154618728587", "-pp1 -x0 3 -go 36 4294957296-4294967295 1", 8)
]

c200 = [
  ("29799904256775982671863388319999573561548825027149399972531599612392671227006866151136667908641695103422986028076864929902803267437351318167549013218980573566942647077444419419003164546362008247462049", "-pm1 2 1e10", 0)
]

test = [
  ("173357946863134423299822098041421951472072119", "-pp1 -x0 5 992599901 1401995848117", 8 ),
]

def run_exe(exe, args, inp) :
  al = {'stdin' : PIPE, 'stdout' : PIPE, 'stderr' : STDOUT }
  if sys.platform.startswith('win') :
    al['creationflags'] = 0x08000000
  p = Popen([exe] + args.split(' '), **al)
  res = p.communicate(inp.encode())[0].decode()
  ret = p.poll()
  return (ret, res)

def do_tests(tests) :
    global out
    exe  = test_dir + "ecm.exe"
    for tt in tests :
        rv = run_exe(exe, tt[1], tt[0])
        if type(tt[2]) == int and rv[0] != tt[2] :
            print("*** ERROR ***", rv[0], tt[2])
        elif type(tt[2]) == tuple and \
                 rv[0] != tt[2][0] and rv[0] != tt[2][2] :
            print("*** ERROR ***", rv[0], tt[2])
        if out :
            op = rv[1].rsplit('\r\n')
            for i in op :
                print(i)
out = True
do_tests(ecm)
do_tests(pm1)
do_tests(pp1)
do_tests(pp1_2)
do_tests(c200)
do_tests(test)
