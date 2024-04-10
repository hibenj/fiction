// Benchmark "mesh8" written by ABC on Tue Apr  9 09:17:15 2024

module mesh8 ( 
    p00x1, p00x2, p00y1, p00y2, p01x1, p01x2, p01y1, p01y2, p02x1, p02x2,
    p02y1, p02y2, p03x1, p03x2, p03y1, p03y2, p04x1, p04x2, p04y1, p04y2,
    p05x1, p05x2, p05y1, p05y2, p06x1, p06x2, p06y1, p06y2, p07x1, p07x2,
    p07y1, p07y2, p10x1, p10x2, p10y1, p10y2, p11x1, p11x2, p11y1, p11y2,
    p12x1, p12x2, p12y1, p12y2, p13x1, p13x2, p13y1, p13y2, p14x1, p14x2,
    p14y1, p14y2, p15x1, p15x2, p15y1, p15y2, p16x1, p16x2, p16y1, p16y2,
    p17x1, p17x2, p17y1, p17y2, p20x1, p20x2, p20y1, p20y2, p21x1, p21x2,
    p21y1, p21y2, p22x1, p22x2, p22y1, p22y2, p23x1, p23x2, p23y1, p23y2,
    p24x1, p24x2, p24y1, p24y2, p25x1, p25x2, p25y1, p25y2, p26x1, p26x2,
    p26y1, p26y2, p27x1, p27x2, p27y1, p27y2, p30x1, p30x2, p30y1, p30y2,
    p31x1, p31x2, p31y1, p31y2, p32x1, p32x2, p32y1, p32y2, p33x1, p33x2,
    p33y1, p33y2, p34x1, p34x2, p34y1, p34y2, p35x1, p35x2, p35y1, p35y2,
    p36x1, p36x2, p36y1, p36y2, p37x1, p37x2, p37y1, p37y2, p40x1, p40x2,
    p40y1, p40y2, p41x1, p41x2, p41y1, p41y2, p42x1, p42x2, p42y1, p42y2,
    p43x1, p43x2, p43y1, p43y2, p44x1, p44x2, p44y1, p44y2, p45x1, p45x2,
    p45y1, p45y2, p46x1, p46x2, p46y1, p46y2, p47x1, p47x2, p47y1, p47y2,
    p50x1, p50x2, p50y1, p50y2, p51x1, p51x2, p51y1, p51y2, p52x1, p52x2,
    p52y1, p52y2, p53x1, p53x2, p53y1, p53y2, p54x1, p54x2, p54y1, p54y2,
    p55x1, p55x2, p55y1, p55y2, p56x1, p56x2, p56y1, p56y2, p57x1, p57x2,
    p57y1, p57y2, p60x1, p60x2, p60y1, p60y2, p61x1, p61x2, p61y1, p61y2,
    p62x1, p62x2, p62y1, p62y2, p63x1, p63x2, p63y1, p63y2, p64x1, p64x2,
    p64y1, p64y2, p65x1, p65x2, p65y1, p65y2, p66x1, p66x2, p66y1, p66y2,
    p67x1, p67x2, p67y1, p67y2, p70x1, p70x2, p70y1, p70y2, p71x1, p71x2,
    p71y1, p71y2, p72x1, p72x2, p72y1, p72y2, p73x1, p73x2, p73y1, p73y2,
    p74x1, p74x2, p74y1, p74y2, p75x1, p75x2, p75y1, p75y2, p76x1, p76x2,
    p76y1, p76y2, p77x1, p77x2, p77y1, p77y2, v00, v01, v02, v03, v04, v05,
    v06, v07, v08, v09, v10, v11, v12, v13, v14, v15,
    fx00  );
  input  p00x1, p00x2, p00y1, p00y2, p01x1, p01x2, p01y1, p01y2, p02x1,
    p02x2, p02y1, p02y2, p03x1, p03x2, p03y1, p03y2, p04x1, p04x2, p04y1,
    p04y2, p05x1, p05x2, p05y1, p05y2, p06x1, p06x2, p06y1, p06y2, p07x1,
    p07x2, p07y1, p07y2, p10x1, p10x2, p10y1, p10y2, p11x1, p11x2, p11y1,
    p11y2, p12x1, p12x2, p12y1, p12y2, p13x1, p13x2, p13y1, p13y2, p14x1,
    p14x2, p14y1, p14y2, p15x1, p15x2, p15y1, p15y2, p16x1, p16x2, p16y1,
    p16y2, p17x1, p17x2, p17y1, p17y2, p20x1, p20x2, p20y1, p20y2, p21x1,
    p21x2, p21y1, p21y2, p22x1, p22x2, p22y1, p22y2, p23x1, p23x2, p23y1,
    p23y2, p24x1, p24x2, p24y1, p24y2, p25x1, p25x2, p25y1, p25y2, p26x1,
    p26x2, p26y1, p26y2, p27x1, p27x2, p27y1, p27y2, p30x1, p30x2, p30y1,
    p30y2, p31x1, p31x2, p31y1, p31y2, p32x1, p32x2, p32y1, p32y2, p33x1,
    p33x2, p33y1, p33y2, p34x1, p34x2, p34y1, p34y2, p35x1, p35x2, p35y1,
    p35y2, p36x1, p36x2, p36y1, p36y2, p37x1, p37x2, p37y1, p37y2, p40x1,
    p40x2, p40y1, p40y2, p41x1, p41x2, p41y1, p41y2, p42x1, p42x2, p42y1,
    p42y2, p43x1, p43x2, p43y1, p43y2, p44x1, p44x2, p44y1, p44y2, p45x1,
    p45x2, p45y1, p45y2, p46x1, p46x2, p46y1, p46y2, p47x1, p47x2, p47y1,
    p47y2, p50x1, p50x2, p50y1, p50y2, p51x1, p51x2, p51y1, p51y2, p52x1,
    p52x2, p52y1, p52y2, p53x1, p53x2, p53y1, p53y2, p54x1, p54x2, p54y1,
    p54y2, p55x1, p55x2, p55y1, p55y2, p56x1, p56x2, p56y1, p56y2, p57x1,
    p57x2, p57y1, p57y2, p60x1, p60x2, p60y1, p60y2, p61x1, p61x2, p61y1,
    p61y2, p62x1, p62x2, p62y1, p62y2, p63x1, p63x2, p63y1, p63y2, p64x1,
    p64x2, p64y1, p64y2, p65x1, p65x2, p65y1, p65y2, p66x1, p66x2, p66y1,
    p66y2, p67x1, p67x2, p67y1, p67y2, p70x1, p70x2, p70y1, p70y2, p71x1,
    p71x2, p71y1, p71y2, p72x1, p72x2, p72y1, p72y2, p73x1, p73x2, p73y1,
    p73y2, p74x1, p74x2, p74y1, p74y2, p75x1, p75x2, p75y1, p75y2, p76x1,
    p76x2, p76y1, p76y2, p77x1, p77x2, p77y1, p77y2, v00, v01, v02, v03,
    v04, v05, v06, v07, v08, v09, v10, v11, v12, v13, v14, v15;
  output fx00;
  wire new_n674_, new_n676_, new_n682_, new_n684_, \new_mesh8|fx01_ ,
    new_n690_, new_n692_, \new_mesh8|fx02_ , new_n698_, new_n700_,
    \new_mesh8|fx03_ , new_n706_, new_n708_, \new_mesh8|fx04_ , new_n714_,
    new_n716_, \new_mesh8|fx05_ , new_n722_, new_n724_, \new_mesh8|fx06_ ,
    new_n730_, new_n732_, \new_mesh8|fx07_ , new_n738_, new_n742_,
    \new_mesh8|fy10_ , new_n746_, new_n748_, new_n750_, \new_mesh8|fx11_ ,
    \new_mesh8|fy11_ , new_n754_, new_n756_, new_n758_, \new_mesh8|fx12_ ,
    \new_mesh8|fy12_ , new_n762_, new_n764_, new_n766_, \new_mesh8|fx13_ ,
    \new_mesh8|fy13_ , new_n770_, new_n772_, new_n774_, \new_mesh8|fx14_ ,
    \new_mesh8|fy14_ , new_n778_, new_n780_, new_n782_, \new_mesh8|fx15_ ,
    \new_mesh8|fy15_ , new_n786_, new_n788_, new_n790_, \new_mesh8|fx16_ ,
    \new_mesh8|fy16_ , new_n794_, new_n796_, new_n798_, \new_mesh8|fx17_ ,
    \new_mesh8|fy17_ , new_n802_, new_n806_, \new_mesh8|fy20_ , new_n810_,
    new_n812_, new_n814_, \new_mesh8|fx21_ , \new_mesh8|fy21_ , new_n818_,
    new_n820_, new_n822_, \new_mesh8|fx22_ , \new_mesh8|fy22_ , new_n826_,
    new_n828_, new_n830_, \new_mesh8|fx23_ , \new_mesh8|fy23_ , new_n834_,
    new_n836_, new_n838_, \new_mesh8|fx24_ , \new_mesh8|fy24_ , new_n842_,
    new_n844_, new_n846_, \new_mesh8|fx25_ , \new_mesh8|fy25_ , new_n850_,
    new_n852_, new_n854_, \new_mesh8|fx26_ , \new_mesh8|fy26_ , new_n858_,
    new_n860_, new_n862_, \new_mesh8|fx27_ , \new_mesh8|fy27_ , new_n866_,
    new_n870_, \new_mesh8|fy30_ , new_n874_, new_n876_, new_n878_,
    \new_mesh8|fx31_ , \new_mesh8|fy31_ , new_n882_, new_n884_, new_n886_,
    \new_mesh8|fx32_ , \new_mesh8|fy32_ , new_n890_, new_n892_, new_n894_,
    \new_mesh8|fx33_ , \new_mesh8|fy33_ , new_n898_, new_n900_, new_n902_,
    \new_mesh8|fx34_ , \new_mesh8|fy34_ , new_n906_, new_n908_, new_n910_,
    \new_mesh8|fx35_ , \new_mesh8|fy35_ , new_n914_, new_n916_, new_n918_,
    \new_mesh8|fx36_ , \new_mesh8|fy36_ , new_n922_, new_n924_, new_n926_,
    \new_mesh8|fx37_ , \new_mesh8|fy37_ , new_n930_, new_n934_,
    \new_mesh8|fy40_ , new_n938_, new_n940_, new_n942_, \new_mesh8|fx41_ ,
    \new_mesh8|fy41_ , new_n946_, new_n948_, new_n950_, \new_mesh8|fx42_ ,
    \new_mesh8|fy42_ , new_n954_, new_n956_, new_n958_, \new_mesh8|fx43_ ,
    \new_mesh8|fy43_ , new_n962_, new_n964_, new_n966_, \new_mesh8|fx44_ ,
    \new_mesh8|fy44_ , new_n970_, new_n972_, new_n974_, \new_mesh8|fx45_ ,
    \new_mesh8|fy45_ , new_n978_, new_n980_, new_n982_, \new_mesh8|fx46_ ,
    \new_mesh8|fy46_ , new_n986_, new_n988_, new_n990_, \new_mesh8|fx47_ ,
    \new_mesh8|fy47_ , new_n994_, new_n998_, \new_mesh8|fy50_ , new_n1002_,
    new_n1004_, new_n1006_, \new_mesh8|fx51_ , \new_mesh8|fy51_ ,
    new_n1010_, new_n1012_, new_n1014_, \new_mesh8|fx52_ ,
    \new_mesh8|fy52_ , new_n1018_, new_n1020_, new_n1022_,
    \new_mesh8|fx53_ , \new_mesh8|fy53_ , new_n1026_, new_n1028_,
    new_n1030_, \new_mesh8|fx54_ , \new_mesh8|fy54_ , new_n1034_,
    new_n1036_, new_n1038_, \new_mesh8|fx55_ , \new_mesh8|fy55_ ,
    new_n1042_, new_n1044_, new_n1046_, \new_mesh8|fx56_ ,
    \new_mesh8|fy56_ , new_n1050_, new_n1052_, new_n1054_,
    \new_mesh8|fx57_ , \new_mesh8|fy57_ , new_n1058_, new_n1062_,
    \new_mesh8|fy60_ , new_n1066_, new_n1068_, new_n1070_,
    \new_mesh8|fx61_ , \new_mesh8|fy61_ , new_n1074_, new_n1076_,
    new_n1078_, \new_mesh8|fx62_ , \new_mesh8|fy62_ , new_n1082_,
    new_n1084_, new_n1086_, \new_mesh8|fx63_ , \new_mesh8|fy63_ ,
    new_n1090_, new_n1092_, new_n1094_, \new_mesh8|fx64_ ,
    \new_mesh8|fy64_ , new_n1098_, new_n1100_, new_n1102_,
    \new_mesh8|fx65_ , \new_mesh8|fy65_ , new_n1106_, new_n1108_,
    new_n1110_, \new_mesh8|fx66_ , \new_mesh8|fy66_ , new_n1114_,
    new_n1116_, new_n1118_, \new_mesh8|fx67_ , \new_mesh8|fy67_ ,
    new_n1122_, new_n1126_, \new_mesh8|fy70_ , new_n1130_, new_n1132_,
    new_n1134_, \new_mesh8|fx71_ , \new_mesh8|fy71_ , new_n1138_,
    new_n1140_, new_n1142_, \new_mesh8|fx72_ , \new_mesh8|fy72_ ,
    new_n1146_, new_n1148_, new_n1150_, \new_mesh8|fx73_ ,
    \new_mesh8|fy73_ , new_n1154_, new_n1156_, new_n1158_,
    \new_mesh8|fx74_ , \new_mesh8|fy74_ , new_n1162_, new_n1164_,
    new_n1166_, \new_mesh8|fx75_ , \new_mesh8|fy75_ , new_n1170_,
    new_n1172_, new_n1174_, \new_mesh8|fx76_ , \new_mesh8|fy76_ ,
    new_n1178_, new_n1180_, new_n1182_, \new_mesh8|fx77_ ,
    \new_mesh8|fy77_ ;
  assign new_n674_ = \new_mesh8|fx01_  & \new_mesh8|fy10_ ;
  assign new_n676_ = p00x1 ? new_n674_ : \new_mesh8|fx01_ ;
  assign fx00 = ~p00x2 ^ ~new_n676_;
  assign new_n682_ = \new_mesh8|fx02_  & \new_mesh8|fy11_ ;
  assign new_n684_ = p01x1 ? new_n682_ : \new_mesh8|fx02_ ;
  assign \new_mesh8|fx01_  = ~p01x2 ^ ~new_n684_;
  assign new_n690_ = \new_mesh8|fx03_  & \new_mesh8|fy12_ ;
  assign new_n692_ = p02x1 ? new_n690_ : \new_mesh8|fx03_ ;
  assign \new_mesh8|fx02_  = ~p02x2 ^ ~new_n692_;
  assign new_n698_ = \new_mesh8|fx04_  & \new_mesh8|fy13_ ;
  assign new_n700_ = p03x1 ? new_n698_ : \new_mesh8|fx04_ ;
  assign \new_mesh8|fx03_  = ~p03x2 ^ ~new_n700_;
  assign new_n706_ = \new_mesh8|fx05_  & \new_mesh8|fy14_ ;
  assign new_n708_ = p04x1 ? new_n706_ : \new_mesh8|fx05_ ;
  assign \new_mesh8|fx04_  = ~p04x2 ^ ~new_n708_;
  assign new_n714_ = \new_mesh8|fx06_  & \new_mesh8|fy15_ ;
  assign new_n716_ = p05x1 ? new_n714_ : \new_mesh8|fx06_ ;
  assign \new_mesh8|fx05_  = ~p05x2 ^ ~new_n716_;
  assign new_n722_ = \new_mesh8|fx07_  & \new_mesh8|fy16_ ;
  assign new_n724_ = p06x1 ? new_n722_ : \new_mesh8|fx07_ ;
  assign \new_mesh8|fx06_  = ~p06x2 ^ ~new_n724_;
  assign new_n730_ = v00 & \new_mesh8|fy17_ ;
  assign new_n732_ = p07x1 ? new_n730_ : v00;
  assign \new_mesh8|fx07_  = ~p07x2 ^ ~new_n732_;
  assign new_n738_ = \new_mesh8|fx11_  & \new_mesh8|fy20_ ;
  assign new_n742_ = p10y1 ? new_n738_ : \new_mesh8|fy20_ ;
  assign \new_mesh8|fy10_  = ~p10y2 ^ ~new_n742_;
  assign new_n746_ = \new_mesh8|fx12_  & \new_mesh8|fy21_ ;
  assign new_n748_ = p11x1 ? new_n746_ : \new_mesh8|fx12_ ;
  assign new_n750_ = p11y1 ? new_n746_ : \new_mesh8|fy21_ ;
  assign \new_mesh8|fx11_  = ~p11x2 ^ ~new_n748_;
  assign \new_mesh8|fy11_  = ~p11y2 ^ ~new_n750_;
  assign new_n754_ = \new_mesh8|fx13_  & \new_mesh8|fy22_ ;
  assign new_n756_ = p12x1 ? new_n754_ : \new_mesh8|fx13_ ;
  assign new_n758_ = p12y1 ? new_n754_ : \new_mesh8|fy22_ ;
  assign \new_mesh8|fx12_  = ~p12x2 ^ ~new_n756_;
  assign \new_mesh8|fy12_  = ~p12y2 ^ ~new_n758_;
  assign new_n762_ = \new_mesh8|fx14_  & \new_mesh8|fy23_ ;
  assign new_n764_ = p13x1 ? new_n762_ : \new_mesh8|fx14_ ;
  assign new_n766_ = p13y1 ? new_n762_ : \new_mesh8|fy23_ ;
  assign \new_mesh8|fx13_  = ~p13x2 ^ ~new_n764_;
  assign \new_mesh8|fy13_  = ~p13y2 ^ ~new_n766_;
  assign new_n770_ = \new_mesh8|fx15_  & \new_mesh8|fy24_ ;
  assign new_n772_ = p14x1 ? new_n770_ : \new_mesh8|fx15_ ;
  assign new_n774_ = p14y1 ? new_n770_ : \new_mesh8|fy24_ ;
  assign \new_mesh8|fx14_  = ~p14x2 ^ ~new_n772_;
  assign \new_mesh8|fy14_  = ~p14y2 ^ ~new_n774_;
  assign new_n778_ = \new_mesh8|fx16_  & \new_mesh8|fy25_ ;
  assign new_n780_ = p15x1 ? new_n778_ : \new_mesh8|fx16_ ;
  assign new_n782_ = p15y1 ? new_n778_ : \new_mesh8|fy25_ ;
  assign \new_mesh8|fx15_  = ~p15x2 ^ ~new_n780_;
  assign \new_mesh8|fy15_  = ~p15y2 ^ ~new_n782_;
  assign new_n786_ = \new_mesh8|fx17_  & \new_mesh8|fy26_ ;
  assign new_n788_ = p16x1 ? new_n786_ : \new_mesh8|fx17_ ;
  assign new_n790_ = p16y1 ? new_n786_ : \new_mesh8|fy26_ ;
  assign \new_mesh8|fx16_  = ~p16x2 ^ ~new_n788_;
  assign \new_mesh8|fy16_  = ~p16y2 ^ ~new_n790_;
  assign new_n794_ = v01 & \new_mesh8|fy27_ ;
  assign new_n796_ = p17x1 ? new_n794_ : v01;
  assign new_n798_ = p17y1 ? new_n794_ : \new_mesh8|fy27_ ;
  assign \new_mesh8|fx17_  = ~p17x2 ^ ~new_n796_;
  assign \new_mesh8|fy17_  = ~p17y2 ^ ~new_n798_;
  assign new_n802_ = \new_mesh8|fx21_  & \new_mesh8|fy30_ ;
  assign new_n806_ = p20y1 ? new_n802_ : \new_mesh8|fy30_ ;
  assign \new_mesh8|fy20_  = ~p20y2 ^ ~new_n806_;
  assign new_n810_ = \new_mesh8|fx22_  & \new_mesh8|fy31_ ;
  assign new_n812_ = p21x1 ? new_n810_ : \new_mesh8|fx22_ ;
  assign new_n814_ = p21y1 ? new_n810_ : \new_mesh8|fy31_ ;
  assign \new_mesh8|fx21_  = ~p21x2 ^ ~new_n812_;
  assign \new_mesh8|fy21_  = ~p21y2 ^ ~new_n814_;
  assign new_n818_ = \new_mesh8|fx23_  & \new_mesh8|fy32_ ;
  assign new_n820_ = p22x1 ? new_n818_ : \new_mesh8|fx23_ ;
  assign new_n822_ = p22y1 ? new_n818_ : \new_mesh8|fy32_ ;
  assign \new_mesh8|fx22_  = ~p22x2 ^ ~new_n820_;
  assign \new_mesh8|fy22_  = ~p22y2 ^ ~new_n822_;
  assign new_n826_ = \new_mesh8|fx24_  & \new_mesh8|fy33_ ;
  assign new_n828_ = p23x1 ? new_n826_ : \new_mesh8|fx24_ ;
  assign new_n830_ = p23y1 ? new_n826_ : \new_mesh8|fy33_ ;
  assign \new_mesh8|fx23_  = ~p23x2 ^ ~new_n828_;
  assign \new_mesh8|fy23_  = ~p23y2 ^ ~new_n830_;
  assign new_n834_ = \new_mesh8|fx25_  & \new_mesh8|fy34_ ;
  assign new_n836_ = p24x1 ? new_n834_ : \new_mesh8|fx25_ ;
  assign new_n838_ = p24y1 ? new_n834_ : \new_mesh8|fy34_ ;
  assign \new_mesh8|fx24_  = ~p24x2 ^ ~new_n836_;
  assign \new_mesh8|fy24_  = ~p24y2 ^ ~new_n838_;
  assign new_n842_ = \new_mesh8|fx26_  & \new_mesh8|fy35_ ;
  assign new_n844_ = p25x1 ? new_n842_ : \new_mesh8|fx26_ ;
  assign new_n846_ = p25y1 ? new_n842_ : \new_mesh8|fy35_ ;
  assign \new_mesh8|fx25_  = ~p25x2 ^ ~new_n844_;
  assign \new_mesh8|fy25_  = ~p25y2 ^ ~new_n846_;
  assign new_n850_ = \new_mesh8|fx27_  & \new_mesh8|fy36_ ;
  assign new_n852_ = p26x1 ? new_n850_ : \new_mesh8|fx27_ ;
  assign new_n854_ = p26y1 ? new_n850_ : \new_mesh8|fy36_ ;
  assign \new_mesh8|fx26_  = ~p26x2 ^ ~new_n852_;
  assign \new_mesh8|fy26_  = ~p26y2 ^ ~new_n854_;
  assign new_n858_ = v02 & \new_mesh8|fy37_ ;
  assign new_n860_ = p27x1 ? new_n858_ : v02;
  assign new_n862_ = p27y1 ? new_n858_ : \new_mesh8|fy37_ ;
  assign \new_mesh8|fx27_  = ~p27x2 ^ ~new_n860_;
  assign \new_mesh8|fy27_  = ~p27y2 ^ ~new_n862_;
  assign new_n866_ = \new_mesh8|fx31_  & \new_mesh8|fy40_ ;
  assign new_n870_ = p30y1 ? new_n866_ : \new_mesh8|fy40_ ;
  assign \new_mesh8|fy30_  = ~p30y2 ^ ~new_n870_;
  assign new_n874_ = \new_mesh8|fx32_  & \new_mesh8|fy41_ ;
  assign new_n876_ = p31x1 ? new_n874_ : \new_mesh8|fx32_ ;
  assign new_n878_ = p31y1 ? new_n874_ : \new_mesh8|fy41_ ;
  assign \new_mesh8|fx31_  = ~p31x2 ^ ~new_n876_;
  assign \new_mesh8|fy31_  = ~p31y2 ^ ~new_n878_;
  assign new_n882_ = \new_mesh8|fx33_  & \new_mesh8|fy42_ ;
  assign new_n884_ = p32x1 ? new_n882_ : \new_mesh8|fx33_ ;
  assign new_n886_ = p32y1 ? new_n882_ : \new_mesh8|fy42_ ;
  assign \new_mesh8|fx32_  = ~p32x2 ^ ~new_n884_;
  assign \new_mesh8|fy32_  = ~p32y2 ^ ~new_n886_;
  assign new_n890_ = \new_mesh8|fx34_  & \new_mesh8|fy43_ ;
  assign new_n892_ = p33x1 ? new_n890_ : \new_mesh8|fx34_ ;
  assign new_n894_ = p33y1 ? new_n890_ : \new_mesh8|fy43_ ;
  assign \new_mesh8|fx33_  = ~p33x2 ^ ~new_n892_;
  assign \new_mesh8|fy33_  = ~p33y2 ^ ~new_n894_;
  assign new_n898_ = \new_mesh8|fx35_  & \new_mesh8|fy44_ ;
  assign new_n900_ = p34x1 ? new_n898_ : \new_mesh8|fx35_ ;
  assign new_n902_ = p34y1 ? new_n898_ : \new_mesh8|fy44_ ;
  assign \new_mesh8|fx34_  = ~p34x2 ^ ~new_n900_;
  assign \new_mesh8|fy34_  = ~p34y2 ^ ~new_n902_;
  assign new_n906_ = \new_mesh8|fx36_  & \new_mesh8|fy45_ ;
  assign new_n908_ = p35x1 ? new_n906_ : \new_mesh8|fx36_ ;
  assign new_n910_ = p35y1 ? new_n906_ : \new_mesh8|fy45_ ;
  assign \new_mesh8|fx35_  = ~p35x2 ^ ~new_n908_;
  assign \new_mesh8|fy35_  = ~p35y2 ^ ~new_n910_;
  assign new_n914_ = \new_mesh8|fx37_  & \new_mesh8|fy46_ ;
  assign new_n916_ = p36x1 ? new_n914_ : \new_mesh8|fx37_ ;
  assign new_n918_ = p36y1 ? new_n914_ : \new_mesh8|fy46_ ;
  assign \new_mesh8|fx36_  = ~p36x2 ^ ~new_n916_;
  assign \new_mesh8|fy36_  = ~p36y2 ^ ~new_n918_;
  assign new_n922_ = v03 & \new_mesh8|fy47_ ;
  assign new_n924_ = p37x1 ? new_n922_ : v03;
  assign new_n926_ = p37y1 ? new_n922_ : \new_mesh8|fy47_ ;
  assign \new_mesh8|fx37_  = ~p37x2 ^ ~new_n924_;
  assign \new_mesh8|fy37_  = ~p37y2 ^ ~new_n926_;
  assign new_n930_ = \new_mesh8|fx41_  & \new_mesh8|fy50_ ;
  assign new_n934_ = p40y1 ? new_n930_ : \new_mesh8|fy50_ ;
  assign \new_mesh8|fy40_  = ~p40y2 ^ ~new_n934_;
  assign new_n938_ = \new_mesh8|fx42_  & \new_mesh8|fy51_ ;
  assign new_n940_ = p41x1 ? new_n938_ : \new_mesh8|fx42_ ;
  assign new_n942_ = p41y1 ? new_n938_ : \new_mesh8|fy51_ ;
  assign \new_mesh8|fx41_  = ~p41x2 ^ ~new_n940_;
  assign \new_mesh8|fy41_  = ~p41y2 ^ ~new_n942_;
  assign new_n946_ = \new_mesh8|fx43_  & \new_mesh8|fy52_ ;
  assign new_n948_ = p42x1 ? new_n946_ : \new_mesh8|fx43_ ;
  assign new_n950_ = p42y1 ? new_n946_ : \new_mesh8|fy52_ ;
  assign \new_mesh8|fx42_  = ~p42x2 ^ ~new_n948_;
  assign \new_mesh8|fy42_  = ~p42y2 ^ ~new_n950_;
  assign new_n954_ = \new_mesh8|fx44_  & \new_mesh8|fy53_ ;
  assign new_n956_ = p43x1 ? new_n954_ : \new_mesh8|fx44_ ;
  assign new_n958_ = p43y1 ? new_n954_ : \new_mesh8|fy53_ ;
  assign \new_mesh8|fx43_  = ~p43x2 ^ ~new_n956_;
  assign \new_mesh8|fy43_  = ~p43y2 ^ ~new_n958_;
  assign new_n962_ = \new_mesh8|fx45_  & \new_mesh8|fy54_ ;
  assign new_n964_ = p44x1 ? new_n962_ : \new_mesh8|fx45_ ;
  assign new_n966_ = p44y1 ? new_n962_ : \new_mesh8|fy54_ ;
  assign \new_mesh8|fx44_  = ~p44x2 ^ ~new_n964_;
  assign \new_mesh8|fy44_  = ~p44y2 ^ ~new_n966_;
  assign new_n970_ = \new_mesh8|fx46_  & \new_mesh8|fy55_ ;
  assign new_n972_ = p45x1 ? new_n970_ : \new_mesh8|fx46_ ;
  assign new_n974_ = p45y1 ? new_n970_ : \new_mesh8|fy55_ ;
  assign \new_mesh8|fx45_  = ~p45x2 ^ ~new_n972_;
  assign \new_mesh8|fy45_  = ~p45y2 ^ ~new_n974_;
  assign new_n978_ = \new_mesh8|fx47_  & \new_mesh8|fy56_ ;
  assign new_n980_ = p46x1 ? new_n978_ : \new_mesh8|fx47_ ;
  assign new_n982_ = p46y1 ? new_n978_ : \new_mesh8|fy56_ ;
  assign \new_mesh8|fx46_  = ~p46x2 ^ ~new_n980_;
  assign \new_mesh8|fy46_  = ~p46y2 ^ ~new_n982_;
  assign new_n986_ = v04 & \new_mesh8|fy57_ ;
  assign new_n988_ = p47x1 ? new_n986_ : v04;
  assign new_n990_ = p47y1 ? new_n986_ : \new_mesh8|fy57_ ;
  assign \new_mesh8|fx47_  = ~p47x2 ^ ~new_n988_;
  assign \new_mesh8|fy47_  = ~p47y2 ^ ~new_n990_;
  assign new_n994_ = \new_mesh8|fx51_  & \new_mesh8|fy60_ ;
  assign new_n998_ = p50y1 ? new_n994_ : \new_mesh8|fy60_ ;
  assign \new_mesh8|fy50_  = ~p50y2 ^ ~new_n998_;
  assign new_n1002_ = \new_mesh8|fx52_  & \new_mesh8|fy61_ ;
  assign new_n1004_ = p51x1 ? new_n1002_ : \new_mesh8|fx52_ ;
  assign new_n1006_ = p51y1 ? new_n1002_ : \new_mesh8|fy61_ ;
  assign \new_mesh8|fx51_  = ~p51x2 ^ ~new_n1004_;
  assign \new_mesh8|fy51_  = ~p51y2 ^ ~new_n1006_;
  assign new_n1010_ = \new_mesh8|fx53_  & \new_mesh8|fy62_ ;
  assign new_n1012_ = p52x1 ? new_n1010_ : \new_mesh8|fx53_ ;
  assign new_n1014_ = p52y1 ? new_n1010_ : \new_mesh8|fy62_ ;
  assign \new_mesh8|fx52_  = ~p52x2 ^ ~new_n1012_;
  assign \new_mesh8|fy52_  = ~p52y2 ^ ~new_n1014_;
  assign new_n1018_ = \new_mesh8|fx54_  & \new_mesh8|fy63_ ;
  assign new_n1020_ = p53x1 ? new_n1018_ : \new_mesh8|fx54_ ;
  assign new_n1022_ = p53y1 ? new_n1018_ : \new_mesh8|fy63_ ;
  assign \new_mesh8|fx53_  = ~p53x2 ^ ~new_n1020_;
  assign \new_mesh8|fy53_  = ~p53y2 ^ ~new_n1022_;
  assign new_n1026_ = \new_mesh8|fx55_  & \new_mesh8|fy64_ ;
  assign new_n1028_ = p54x1 ? new_n1026_ : \new_mesh8|fx55_ ;
  assign new_n1030_ = p54y1 ? new_n1026_ : \new_mesh8|fy64_ ;
  assign \new_mesh8|fx54_  = ~p54x2 ^ ~new_n1028_;
  assign \new_mesh8|fy54_  = ~p54y2 ^ ~new_n1030_;
  assign new_n1034_ = \new_mesh8|fx56_  & \new_mesh8|fy65_ ;
  assign new_n1036_ = p55x1 ? new_n1034_ : \new_mesh8|fx56_ ;
  assign new_n1038_ = p55y1 ? new_n1034_ : \new_mesh8|fy65_ ;
  assign \new_mesh8|fx55_  = ~p55x2 ^ ~new_n1036_;
  assign \new_mesh8|fy55_  = ~p55y2 ^ ~new_n1038_;
  assign new_n1042_ = \new_mesh8|fx57_  & \new_mesh8|fy66_ ;
  assign new_n1044_ = p56x1 ? new_n1042_ : \new_mesh8|fx57_ ;
  assign new_n1046_ = p56y1 ? new_n1042_ : \new_mesh8|fy66_ ;
  assign \new_mesh8|fx56_  = ~p56x2 ^ ~new_n1044_;
  assign \new_mesh8|fy56_  = ~p56y2 ^ ~new_n1046_;
  assign new_n1050_ = v05 & \new_mesh8|fy67_ ;
  assign new_n1052_ = p57x1 ? new_n1050_ : v05;
  assign new_n1054_ = p57y1 ? new_n1050_ : \new_mesh8|fy67_ ;
  assign \new_mesh8|fx57_  = ~p57x2 ^ ~new_n1052_;
  assign \new_mesh8|fy57_  = ~p57y2 ^ ~new_n1054_;
  assign new_n1058_ = \new_mesh8|fx61_  & \new_mesh8|fy70_ ;
  assign new_n1062_ = p60y1 ? new_n1058_ : \new_mesh8|fy70_ ;
  assign \new_mesh8|fy60_  = ~p60y2 ^ ~new_n1062_;
  assign new_n1066_ = \new_mesh8|fx62_  & \new_mesh8|fy71_ ;
  assign new_n1068_ = p61x1 ? new_n1066_ : \new_mesh8|fx62_ ;
  assign new_n1070_ = p61y1 ? new_n1066_ : \new_mesh8|fy71_ ;
  assign \new_mesh8|fx61_  = ~p61x2 ^ ~new_n1068_;
  assign \new_mesh8|fy61_  = ~p61y2 ^ ~new_n1070_;
  assign new_n1074_ = \new_mesh8|fx63_  & \new_mesh8|fy72_ ;
  assign new_n1076_ = p62x1 ? new_n1074_ : \new_mesh8|fx63_ ;
  assign new_n1078_ = p62y1 ? new_n1074_ : \new_mesh8|fy72_ ;
  assign \new_mesh8|fx62_  = ~p62x2 ^ ~new_n1076_;
  assign \new_mesh8|fy62_  = ~p62y2 ^ ~new_n1078_;
  assign new_n1082_ = \new_mesh8|fx64_  & \new_mesh8|fy73_ ;
  assign new_n1084_ = p63x1 ? new_n1082_ : \new_mesh8|fx64_ ;
  assign new_n1086_ = p63y1 ? new_n1082_ : \new_mesh8|fy73_ ;
  assign \new_mesh8|fx63_  = ~p63x2 ^ ~new_n1084_;
  assign \new_mesh8|fy63_  = ~p63y2 ^ ~new_n1086_;
  assign new_n1090_ = \new_mesh8|fx65_  & \new_mesh8|fy74_ ;
  assign new_n1092_ = p64x1 ? new_n1090_ : \new_mesh8|fx65_ ;
  assign new_n1094_ = p64y1 ? new_n1090_ : \new_mesh8|fy74_ ;
  assign \new_mesh8|fx64_  = ~p64x2 ^ ~new_n1092_;
  assign \new_mesh8|fy64_  = ~p64y2 ^ ~new_n1094_;
  assign new_n1098_ = \new_mesh8|fx66_  & \new_mesh8|fy75_ ;
  assign new_n1100_ = p65x1 ? new_n1098_ : \new_mesh8|fx66_ ;
  assign new_n1102_ = p65y1 ? new_n1098_ : \new_mesh8|fy75_ ;
  assign \new_mesh8|fx65_  = ~p65x2 ^ ~new_n1100_;
  assign \new_mesh8|fy65_  = ~p65y2 ^ ~new_n1102_;
  assign new_n1106_ = \new_mesh8|fx67_  & \new_mesh8|fy76_ ;
  assign new_n1108_ = p66x1 ? new_n1106_ : \new_mesh8|fx67_ ;
  assign new_n1110_ = p66y1 ? new_n1106_ : \new_mesh8|fy76_ ;
  assign \new_mesh8|fx66_  = ~p66x2 ^ ~new_n1108_;
  assign \new_mesh8|fy66_  = ~p66y2 ^ ~new_n1110_;
  assign new_n1114_ = v06 & \new_mesh8|fy77_ ;
  assign new_n1116_ = p67x1 ? new_n1114_ : v06;
  assign new_n1118_ = p67y1 ? new_n1114_ : \new_mesh8|fy77_ ;
  assign \new_mesh8|fx67_  = ~p67x2 ^ ~new_n1116_;
  assign \new_mesh8|fy67_  = ~p67y2 ^ ~new_n1118_;
  assign new_n1122_ = \new_mesh8|fx71_  & v08;
  assign new_n1126_ = p70y1 ? new_n1122_ : v08;
  assign \new_mesh8|fy70_  = ~p70y2 ^ ~new_n1126_;
  assign new_n1130_ = \new_mesh8|fx72_  & v09;
  assign new_n1132_ = p71x1 ? new_n1130_ : \new_mesh8|fx72_ ;
  assign new_n1134_ = p71y1 ? new_n1130_ : v09;
  assign \new_mesh8|fx71_  = ~p71x2 ^ ~new_n1132_;
  assign \new_mesh8|fy71_  = ~p71y2 ^ ~new_n1134_;
  assign new_n1138_ = \new_mesh8|fx73_  & v10;
  assign new_n1140_ = p72x1 ? new_n1138_ : \new_mesh8|fx73_ ;
  assign new_n1142_ = p72y1 ? new_n1138_ : v10;
  assign \new_mesh8|fx72_  = ~p72x2 ^ ~new_n1140_;
  assign \new_mesh8|fy72_  = ~p72y2 ^ ~new_n1142_;
  assign new_n1146_ = \new_mesh8|fx74_  & v11;
  assign new_n1148_ = p73x1 ? new_n1146_ : \new_mesh8|fx74_ ;
  assign new_n1150_ = p73y1 ? new_n1146_ : v11;
  assign \new_mesh8|fx73_  = ~p73x2 ^ ~new_n1148_;
  assign \new_mesh8|fy73_  = ~p73y2 ^ ~new_n1150_;
  assign new_n1154_ = \new_mesh8|fx75_  & v12;
  assign new_n1156_ = p74x1 ? new_n1154_ : \new_mesh8|fx75_ ;
  assign new_n1158_ = p74y1 ? new_n1154_ : v12;
  assign \new_mesh8|fx74_  = ~p74x2 ^ ~new_n1156_;
  assign \new_mesh8|fy74_  = ~p74y2 ^ ~new_n1158_;
  assign new_n1162_ = \new_mesh8|fx76_  & v13;
  assign new_n1164_ = p75x1 ? new_n1162_ : \new_mesh8|fx76_ ;
  assign new_n1166_ = p75y1 ? new_n1162_ : v13;
  assign \new_mesh8|fx75_  = ~p75x2 ^ ~new_n1164_;
  assign \new_mesh8|fy75_  = ~p75y2 ^ ~new_n1166_;
  assign new_n1170_ = \new_mesh8|fx77_  & v14;
  assign new_n1172_ = p76x1 ? new_n1170_ : \new_mesh8|fx77_ ;
  assign new_n1174_ = p76y1 ? new_n1170_ : v14;
  assign \new_mesh8|fx76_  = ~p76x2 ^ ~new_n1172_;
  assign \new_mesh8|fy76_  = ~p76y2 ^ ~new_n1174_;
  assign new_n1178_ = v07 & v15;
  assign new_n1180_ = p77x1 ? new_n1178_ : v07;
  assign new_n1182_ = p77y1 ? new_n1178_ : v15;
  assign \new_mesh8|fx77_  = ~p77x2 ^ ~new_n1180_;
  assign \new_mesh8|fy77_  = ~p77y2 ^ ~new_n1182_;
endmodule


