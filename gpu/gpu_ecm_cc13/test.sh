#!/bin/sh

N=2222285580939429189199633409709
N1=65798732165875434667
N2=123456793243254987954321549865798732165875434657
N3=29799904256775982671863388319999573561548825027149399972531599612392671227006866151136667908641695103422986028076864929902803267437351318167549013218980573566942647077444419419003164546362008247462049
N1024=476816130611465936450527290044359087291270430228588961857108620206604507094389278927029743389558154792732156255651070582152699963905567360471846264913608076336028875141504121894330002799023810526719040913178785336535694991836095292320878964120315345601424858303342011304569249096484197469497769115553

B1=500
curves=144

time ./gpu_ecm $N $B1 -n $curves -s 17
