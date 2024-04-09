// Benchmark "mesh8" written by ABC on Tue Apr  9 10:22:47 2024

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
  wire new_new_n674__, new_new_n676__, new_new_n682__, new_new_n684__,
    \new_new_mesh8|fx01__ , new_new_n690__, new_new_n692__,
    \new_new_mesh8|fx02__ , new_new_n698__, new_new_n700__,
    \new_new_mesh8|fx03__ , new_new_n706__, new_new_n708__,
    \new_new_mesh8|fx04__ , new_new_n714__, new_new_n716__,
    \new_new_mesh8|fx05__ , new_new_n722__, new_new_n724__,
    \new_new_mesh8|fx06__ , new_new_n730__, new_new_n732__,
    \new_new_mesh8|fx07__ , new_new_n738__, new_new_n742__,
    \new_new_mesh8|fy10__ , new_new_n746__, new_new_n748__, new_new_n750__,
    \new_new_mesh8|fx11__ , \new_new_mesh8|fy11__ , new_new_n754__,
    new_new_n756__, new_new_n758__, \new_new_mesh8|fx12__ ,
    \new_new_mesh8|fy12__ , new_new_n762__, new_new_n764__, new_new_n766__,
    \new_new_mesh8|fx13__ , \new_new_mesh8|fy13__ , new_new_n770__,
    new_new_n772__, new_new_n774__, \new_new_mesh8|fx14__ ,
    \new_new_mesh8|fy14__ , new_new_n778__, new_new_n780__, new_new_n782__,
    \new_new_mesh8|fx15__ , \new_new_mesh8|fy15__ , new_new_n786__,
    new_new_n788__, new_new_n790__, \new_new_mesh8|fx16__ ,
    \new_new_mesh8|fy16__ , new_new_n794__, new_new_n796__, new_new_n798__,
    \new_new_mesh8|fx17__ , \new_new_mesh8|fy17__ , new_new_n802__,
    new_new_n806__, \new_new_mesh8|fy20__ , new_new_n810__, new_new_n812__,
    new_new_n814__, \new_new_mesh8|fx21__ , \new_new_mesh8|fy21__ ,
    new_new_n818__, new_new_n820__, new_new_n822__, \new_new_mesh8|fx22__ ,
    \new_new_mesh8|fy22__ , new_new_n826__, new_new_n828__, new_new_n830__,
    \new_new_mesh8|fx23__ , \new_new_mesh8|fy23__ , new_new_n834__,
    new_new_n836__, new_new_n838__, \new_new_mesh8|fx24__ ,
    \new_new_mesh8|fy24__ , new_new_n842__, new_new_n844__, new_new_n846__,
    \new_new_mesh8|fx25__ , \new_new_mesh8|fy25__ , new_new_n850__,
    new_new_n852__, new_new_n854__, \new_new_mesh8|fx26__ ,
    \new_new_mesh8|fy26__ , new_new_n858__, new_new_n860__, new_new_n862__,
    \new_new_mesh8|fx27__ , \new_new_mesh8|fy27__ , new_new_n866__,
    new_new_n870__, \new_new_mesh8|fy30__ , new_new_n874__, new_new_n876__,
    new_new_n878__, \new_new_mesh8|fx31__ , \new_new_mesh8|fy31__ ,
    new_new_n882__, new_new_n884__, new_new_n886__, \new_new_mesh8|fx32__ ,
    \new_new_mesh8|fy32__ , new_new_n890__, new_new_n892__, new_new_n894__,
    \new_new_mesh8|fx33__ , \new_new_mesh8|fy33__ , new_new_n898__,
    new_new_n900__, new_new_n902__, \new_new_mesh8|fx34__ ,
    \new_new_mesh8|fy34__ , new_new_n906__, new_new_n908__, new_new_n910__,
    \new_new_mesh8|fx35__ , \new_new_mesh8|fy35__ , new_new_n914__,
    new_new_n916__, new_new_n918__, \new_new_mesh8|fx36__ ,
    \new_new_mesh8|fy36__ , new_new_n922__, new_new_n924__, new_new_n926__,
    \new_new_mesh8|fx37__ , \new_new_mesh8|fy37__ , new_new_n930__,
    new_new_n934__, \new_new_mesh8|fy40__ , new_new_n938__, new_new_n940__,
    new_new_n942__, \new_new_mesh8|fx41__ , \new_new_mesh8|fy41__ ,
    new_new_n946__, new_new_n948__, new_new_n950__, \new_new_mesh8|fx42__ ,
    \new_new_mesh8|fy42__ , new_new_n954__, new_new_n956__, new_new_n958__,
    \new_new_mesh8|fx43__ , \new_new_mesh8|fy43__ , new_new_n962__,
    new_new_n964__, new_new_n966__, \new_new_mesh8|fx44__ ,
    \new_new_mesh8|fy44__ , new_new_n970__, new_new_n972__, new_new_n974__,
    \new_new_mesh8|fx45__ , \new_new_mesh8|fy45__ , new_new_n978__,
    new_new_n980__, new_new_n982__, \new_new_mesh8|fx46__ ,
    \new_new_mesh8|fy46__ , new_new_n986__, new_new_n988__, new_new_n990__,
    \new_new_mesh8|fx47__ , \new_new_mesh8|fy47__ , new_new_n994__,
    new_new_n998__, \new_new_mesh8|fy50__ , new_new_n1002__,
    new_new_n1004__, new_new_n1006__, \new_new_mesh8|fx51__ ,
    \new_new_mesh8|fy51__ , new_new_n1010__, new_new_n1012__,
    new_new_n1014__, \new_new_mesh8|fx52__ , \new_new_mesh8|fy52__ ,
    new_new_n1018__, new_new_n1020__, new_new_n1022__,
    \new_new_mesh8|fx53__ , \new_new_mesh8|fy53__ , new_new_n1026__,
    new_new_n1028__, new_new_n1030__, \new_new_mesh8|fx54__ ,
    \new_new_mesh8|fy54__ , new_new_n1034__, new_new_n1036__,
    new_new_n1038__, \new_new_mesh8|fx55__ , \new_new_mesh8|fy55__ ,
    new_new_n1042__, new_new_n1044__, new_new_n1046__,
    \new_new_mesh8|fx56__ , \new_new_mesh8|fy56__ , new_new_n1050__,
    new_new_n1052__, new_new_n1054__, \new_new_mesh8|fx57__ ,
    \new_new_mesh8|fy57__ , new_new_n1058__, new_new_n1062__,
    \new_new_mesh8|fy60__ , new_new_n1066__, new_new_n1068__,
    new_new_n1070__, \new_new_mesh8|fx61__ , \new_new_mesh8|fy61__ ,
    new_new_n1074__, new_new_n1076__, new_new_n1078__,
    \new_new_mesh8|fx62__ , \new_new_mesh8|fy62__ , new_new_n1082__,
    new_new_n1084__, new_new_n1086__, \new_new_mesh8|fx63__ ,
    \new_new_mesh8|fy63__ , new_new_n1090__, new_new_n1092__,
    new_new_n1094__, \new_new_mesh8|fx64__ , \new_new_mesh8|fy64__ ,
    new_new_n1098__, new_new_n1100__, new_new_n1102__,
    \new_new_mesh8|fx65__ , \new_new_mesh8|fy65__ , new_new_n1106__,
    new_new_n1108__, new_new_n1110__, \new_new_mesh8|fx66__ ,
    \new_new_mesh8|fy66__ , new_new_n1114__, new_new_n1116__,
    new_new_n1118__, \new_new_mesh8|fx67__ , \new_new_mesh8|fy67__ ,
    new_new_n1122__, new_new_n1126__, \new_new_mesh8|fy70__ ,
    new_new_n1130__, new_new_n1132__, new_new_n1134__,
    \new_new_mesh8|fx71__ , \new_new_mesh8|fy71__ , new_new_n1138__,
    new_new_n1140__, new_new_n1142__, \new_new_mesh8|fx72__ ,
    \new_new_mesh8|fy72__ , new_new_n1146__, new_new_n1148__,
    new_new_n1150__, \new_new_mesh8|fx73__ , \new_new_mesh8|fy73__ ,
    new_new_n1154__, new_new_n1156__, new_new_n1158__,
    \new_new_mesh8|fx74__ , \new_new_mesh8|fy74__ , new_new_n1162__,
    new_new_n1164__, new_new_n1166__, \new_new_mesh8|fx75__ ,
    \new_new_mesh8|fy75__ , new_new_n1170__, new_new_n1172__,
    new_new_n1174__, \new_new_mesh8|fx76__ , \new_new_mesh8|fy76__ ,
    new_new_n1178__, new_new_n1180__, new_new_n1182__,
    \new_new_mesh8|fx77__ , \new_new_mesh8|fy77__ ;
  assign new_new_n674__ = \new_new_mesh8|fx01__  & \new_new_mesh8|fy10__ ;
  assign new_new_n676__ = p00x1 ? new_new_n674__ : \new_new_mesh8|fx01__ ;
  assign fx00 = ~p00x2 ^ ~new_new_n676__;
  assign new_new_n682__ = \new_new_mesh8|fx02__  & \new_new_mesh8|fy11__ ;
  assign new_new_n684__ = p01x1 ? new_new_n682__ : \new_new_mesh8|fx02__ ;
  assign \new_new_mesh8|fx01__  = ~p01x2 ^ ~new_new_n684__;
  assign new_new_n690__ = \new_new_mesh8|fx03__  & \new_new_mesh8|fy12__ ;
  assign new_new_n692__ = p02x1 ? new_new_n690__ : \new_new_mesh8|fx03__ ;
  assign \new_new_mesh8|fx02__  = ~p02x2 ^ ~new_new_n692__;
  assign new_new_n698__ = \new_new_mesh8|fx04__  & \new_new_mesh8|fy13__ ;
  assign new_new_n700__ = p03x1 ? new_new_n698__ : \new_new_mesh8|fx04__ ;
  assign \new_new_mesh8|fx03__  = ~p03x2 ^ ~new_new_n700__;
  assign new_new_n706__ = \new_new_mesh8|fx05__  & \new_new_mesh8|fy14__ ;
  assign new_new_n708__ = p04x1 ? new_new_n706__ : \new_new_mesh8|fx05__ ;
  assign \new_new_mesh8|fx04__  = ~p04x2 ^ ~new_new_n708__;
  assign new_new_n714__ = \new_new_mesh8|fx06__  & \new_new_mesh8|fy15__ ;
  assign new_new_n716__ = p05x1 ? new_new_n714__ : \new_new_mesh8|fx06__ ;
  assign \new_new_mesh8|fx05__  = ~p05x2 ^ ~new_new_n716__;
  assign new_new_n722__ = \new_new_mesh8|fx07__  & \new_new_mesh8|fy16__ ;
  assign new_new_n724__ = p06x1 ? new_new_n722__ : \new_new_mesh8|fx07__ ;
  assign \new_new_mesh8|fx06__  = ~p06x2 ^ ~new_new_n724__;
  assign new_new_n730__ = v00 & \new_new_mesh8|fy17__ ;
  assign new_new_n732__ = p07x1 ? new_new_n730__ : v00;
  assign \new_new_mesh8|fx07__  = ~p07x2 ^ ~new_new_n732__;
  assign new_new_n738__ = \new_new_mesh8|fx11__  & \new_new_mesh8|fy20__ ;
  assign new_new_n742__ = p10y1 ? new_new_n738__ : \new_new_mesh8|fy20__ ;
  assign \new_new_mesh8|fy10__  = ~p10y2 ^ ~new_new_n742__;
  assign new_new_n746__ = \new_new_mesh8|fx12__  & \new_new_mesh8|fy21__ ;
  assign new_new_n748__ = p11x1 ? new_new_n746__ : \new_new_mesh8|fx12__ ;
  assign new_new_n750__ = p11y1 ? new_new_n746__ : \new_new_mesh8|fy21__ ;
  assign \new_new_mesh8|fx11__  = ~p11x2 ^ ~new_new_n748__;
  assign \new_new_mesh8|fy11__  = ~p11y2 ^ ~new_new_n750__;
  assign new_new_n754__ = \new_new_mesh8|fx13__  & \new_new_mesh8|fy22__ ;
  assign new_new_n756__ = p12x1 ? new_new_n754__ : \new_new_mesh8|fx13__ ;
  assign new_new_n758__ = p12y1 ? new_new_n754__ : \new_new_mesh8|fy22__ ;
  assign \new_new_mesh8|fx12__  = ~p12x2 ^ ~new_new_n756__;
  assign \new_new_mesh8|fy12__  = ~p12y2 ^ ~new_new_n758__;
  assign new_new_n762__ = \new_new_mesh8|fx14__  & \new_new_mesh8|fy23__ ;
  assign new_new_n764__ = p13x1 ? new_new_n762__ : \new_new_mesh8|fx14__ ;
  assign new_new_n766__ = p13y1 ? new_new_n762__ : \new_new_mesh8|fy23__ ;
  assign \new_new_mesh8|fx13__  = ~p13x2 ^ ~new_new_n764__;
  assign \new_new_mesh8|fy13__  = ~p13y2 ^ ~new_new_n766__;
  assign new_new_n770__ = \new_new_mesh8|fx15__  & \new_new_mesh8|fy24__ ;
  assign new_new_n772__ = p14x1 ? new_new_n770__ : \new_new_mesh8|fx15__ ;
  assign new_new_n774__ = p14y1 ? new_new_n770__ : \new_new_mesh8|fy24__ ;
  assign \new_new_mesh8|fx14__  = ~p14x2 ^ ~new_new_n772__;
  assign \new_new_mesh8|fy14__  = ~p14y2 ^ ~new_new_n774__;
  assign new_new_n778__ = \new_new_mesh8|fx16__  & \new_new_mesh8|fy25__ ;
  assign new_new_n780__ = p15x1 ? new_new_n778__ : \new_new_mesh8|fx16__ ;
  assign new_new_n782__ = p15y1 ? new_new_n778__ : \new_new_mesh8|fy25__ ;
  assign \new_new_mesh8|fx15__  = ~p15x2 ^ ~new_new_n780__;
  assign \new_new_mesh8|fy15__  = ~p15y2 ^ ~new_new_n782__;
  assign new_new_n786__ = \new_new_mesh8|fx17__  & \new_new_mesh8|fy26__ ;
  assign new_new_n788__ = p16x1 ? new_new_n786__ : \new_new_mesh8|fx17__ ;
  assign new_new_n790__ = p16y1 ? new_new_n786__ : \new_new_mesh8|fy26__ ;
  assign \new_new_mesh8|fx16__  = ~p16x2 ^ ~new_new_n788__;
  assign \new_new_mesh8|fy16__  = ~p16y2 ^ ~new_new_n790__;
  assign new_new_n794__ = v01 & \new_new_mesh8|fy27__ ;
  assign new_new_n796__ = p17x1 ? new_new_n794__ : v01;
  assign new_new_n798__ = p17y1 ? new_new_n794__ : \new_new_mesh8|fy27__ ;
  assign \new_new_mesh8|fx17__  = ~p17x2 ^ ~new_new_n796__;
  assign \new_new_mesh8|fy17__  = ~p17y2 ^ ~new_new_n798__;
  assign new_new_n802__ = \new_new_mesh8|fx21__  & \new_new_mesh8|fy30__ ;
  assign new_new_n806__ = p20y1 ? new_new_n802__ : \new_new_mesh8|fy30__ ;
  assign \new_new_mesh8|fy20__  = ~p20y2 ^ ~new_new_n806__;
  assign new_new_n810__ = \new_new_mesh8|fx22__  & \new_new_mesh8|fy31__ ;
  assign new_new_n812__ = p21x1 ? new_new_n810__ : \new_new_mesh8|fx22__ ;
  assign new_new_n814__ = p21y1 ? new_new_n810__ : \new_new_mesh8|fy31__ ;
  assign \new_new_mesh8|fx21__  = ~p21x2 ^ ~new_new_n812__;
  assign \new_new_mesh8|fy21__  = ~p21y2 ^ ~new_new_n814__;
  assign new_new_n818__ = \new_new_mesh8|fx23__  & \new_new_mesh8|fy32__ ;
  assign new_new_n820__ = p22x1 ? new_new_n818__ : \new_new_mesh8|fx23__ ;
  assign new_new_n822__ = p22y1 ? new_new_n818__ : \new_new_mesh8|fy32__ ;
  assign \new_new_mesh8|fx22__  = ~p22x2 ^ ~new_new_n820__;
  assign \new_new_mesh8|fy22__  = ~p22y2 ^ ~new_new_n822__;
  assign new_new_n826__ = \new_new_mesh8|fx24__  & \new_new_mesh8|fy33__ ;
  assign new_new_n828__ = p23x1 ? new_new_n826__ : \new_new_mesh8|fx24__ ;
  assign new_new_n830__ = p23y1 ? new_new_n826__ : \new_new_mesh8|fy33__ ;
  assign \new_new_mesh8|fx23__  = ~p23x2 ^ ~new_new_n828__;
  assign \new_new_mesh8|fy23__  = ~p23y2 ^ ~new_new_n830__;
  assign new_new_n834__ = \new_new_mesh8|fx25__  & \new_new_mesh8|fy34__ ;
  assign new_new_n836__ = p24x1 ? new_new_n834__ : \new_new_mesh8|fx25__ ;
  assign new_new_n838__ = p24y1 ? new_new_n834__ : \new_new_mesh8|fy34__ ;
  assign \new_new_mesh8|fx24__  = ~p24x2 ^ ~new_new_n836__;
  assign \new_new_mesh8|fy24__  = ~p24y2 ^ ~new_new_n838__;
  assign new_new_n842__ = \new_new_mesh8|fx26__  & \new_new_mesh8|fy35__ ;
  assign new_new_n844__ = p25x1 ? new_new_n842__ : \new_new_mesh8|fx26__ ;
  assign new_new_n846__ = p25y1 ? new_new_n842__ : \new_new_mesh8|fy35__ ;
  assign \new_new_mesh8|fx25__  = ~p25x2 ^ ~new_new_n844__;
  assign \new_new_mesh8|fy25__  = ~p25y2 ^ ~new_new_n846__;
  assign new_new_n850__ = \new_new_mesh8|fx27__  & \new_new_mesh8|fy36__ ;
  assign new_new_n852__ = p26x1 ? new_new_n850__ : \new_new_mesh8|fx27__ ;
  assign new_new_n854__ = p26y1 ? new_new_n850__ : \new_new_mesh8|fy36__ ;
  assign \new_new_mesh8|fx26__  = ~p26x2 ^ ~new_new_n852__;
  assign \new_new_mesh8|fy26__  = ~p26y2 ^ ~new_new_n854__;
  assign new_new_n858__ = v02 & \new_new_mesh8|fy37__ ;
  assign new_new_n860__ = p27x1 ? new_new_n858__ : v02;
  assign new_new_n862__ = p27y1 ? new_new_n858__ : \new_new_mesh8|fy37__ ;
  assign \new_new_mesh8|fx27__  = ~p27x2 ^ ~new_new_n860__;
  assign \new_new_mesh8|fy27__  = ~p27y2 ^ ~new_new_n862__;
  assign new_new_n866__ = \new_new_mesh8|fx31__  & \new_new_mesh8|fy40__ ;
  assign new_new_n870__ = p30y1 ? new_new_n866__ : \new_new_mesh8|fy40__ ;
  assign \new_new_mesh8|fy30__  = ~p30y2 ^ ~new_new_n870__;
  assign new_new_n874__ = \new_new_mesh8|fx32__  & \new_new_mesh8|fy41__ ;
  assign new_new_n876__ = p31x1 ? new_new_n874__ : \new_new_mesh8|fx32__ ;
  assign new_new_n878__ = p31y1 ? new_new_n874__ : \new_new_mesh8|fy41__ ;
  assign \new_new_mesh8|fx31__  = ~p31x2 ^ ~new_new_n876__;
  assign \new_new_mesh8|fy31__  = ~p31y2 ^ ~new_new_n878__;
  assign new_new_n882__ = \new_new_mesh8|fx33__  & \new_new_mesh8|fy42__ ;
  assign new_new_n884__ = p32x1 ? new_new_n882__ : \new_new_mesh8|fx33__ ;
  assign new_new_n886__ = p32y1 ? new_new_n882__ : \new_new_mesh8|fy42__ ;
  assign \new_new_mesh8|fx32__  = ~p32x2 ^ ~new_new_n884__;
  assign \new_new_mesh8|fy32__  = ~p32y2 ^ ~new_new_n886__;
  assign new_new_n890__ = \new_new_mesh8|fx34__  & \new_new_mesh8|fy43__ ;
  assign new_new_n892__ = p33x1 ? new_new_n890__ : \new_new_mesh8|fx34__ ;
  assign new_new_n894__ = p33y1 ? new_new_n890__ : \new_new_mesh8|fy43__ ;
  assign \new_new_mesh8|fx33__  = ~p33x2 ^ ~new_new_n892__;
  assign \new_new_mesh8|fy33__  = ~p33y2 ^ ~new_new_n894__;
  assign new_new_n898__ = \new_new_mesh8|fx35__  & \new_new_mesh8|fy44__ ;
  assign new_new_n900__ = p34x1 ? new_new_n898__ : \new_new_mesh8|fx35__ ;
  assign new_new_n902__ = p34y1 ? new_new_n898__ : \new_new_mesh8|fy44__ ;
  assign \new_new_mesh8|fx34__  = ~p34x2 ^ ~new_new_n900__;
  assign \new_new_mesh8|fy34__  = ~p34y2 ^ ~new_new_n902__;
  assign new_new_n906__ = \new_new_mesh8|fx36__  & \new_new_mesh8|fy45__ ;
  assign new_new_n908__ = p35x1 ? new_new_n906__ : \new_new_mesh8|fx36__ ;
  assign new_new_n910__ = p35y1 ? new_new_n906__ : \new_new_mesh8|fy45__ ;
  assign \new_new_mesh8|fx35__  = ~p35x2 ^ ~new_new_n908__;
  assign \new_new_mesh8|fy35__  = ~p35y2 ^ ~new_new_n910__;
  assign new_new_n914__ = \new_new_mesh8|fx37__  & \new_new_mesh8|fy46__ ;
  assign new_new_n916__ = p36x1 ? new_new_n914__ : \new_new_mesh8|fx37__ ;
  assign new_new_n918__ = p36y1 ? new_new_n914__ : \new_new_mesh8|fy46__ ;
  assign \new_new_mesh8|fx36__  = ~p36x2 ^ ~new_new_n916__;
  assign \new_new_mesh8|fy36__  = ~p36y2 ^ ~new_new_n918__;
  assign new_new_n922__ = v03 & \new_new_mesh8|fy47__ ;
  assign new_new_n924__ = p37x1 ? new_new_n922__ : v03;
  assign new_new_n926__ = p37y1 ? new_new_n922__ : \new_new_mesh8|fy47__ ;
  assign \new_new_mesh8|fx37__  = ~p37x2 ^ ~new_new_n924__;
  assign \new_new_mesh8|fy37__  = ~p37y2 ^ ~new_new_n926__;
  assign new_new_n930__ = \new_new_mesh8|fx41__  & \new_new_mesh8|fy50__ ;
  assign new_new_n934__ = p40y1 ? new_new_n930__ : \new_new_mesh8|fy50__ ;
  assign \new_new_mesh8|fy40__  = ~p40y2 ^ ~new_new_n934__;
  assign new_new_n938__ = \new_new_mesh8|fx42__  & \new_new_mesh8|fy51__ ;
  assign new_new_n940__ = p41x1 ? new_new_n938__ : \new_new_mesh8|fx42__ ;
  assign new_new_n942__ = p41y1 ? new_new_n938__ : \new_new_mesh8|fy51__ ;
  assign \new_new_mesh8|fx41__  = ~p41x2 ^ ~new_new_n940__;
  assign \new_new_mesh8|fy41__  = ~p41y2 ^ ~new_new_n942__;
  assign new_new_n946__ = \new_new_mesh8|fx43__  & \new_new_mesh8|fy52__ ;
  assign new_new_n948__ = p42x1 ? new_new_n946__ : \new_new_mesh8|fx43__ ;
  assign new_new_n950__ = p42y1 ? new_new_n946__ : \new_new_mesh8|fy52__ ;
  assign \new_new_mesh8|fx42__  = ~p42x2 ^ ~new_new_n948__;
  assign \new_new_mesh8|fy42__  = ~p42y2 ^ ~new_new_n950__;
  assign new_new_n954__ = \new_new_mesh8|fx44__  & \new_new_mesh8|fy53__ ;
  assign new_new_n956__ = p43x1 ? new_new_n954__ : \new_new_mesh8|fx44__ ;
  assign new_new_n958__ = p43y1 ? new_new_n954__ : \new_new_mesh8|fy53__ ;
  assign \new_new_mesh8|fx43__  = ~p43x2 ^ ~new_new_n956__;
  assign \new_new_mesh8|fy43__  = ~p43y2 ^ ~new_new_n958__;
  assign new_new_n962__ = \new_new_mesh8|fx45__  & \new_new_mesh8|fy54__ ;
  assign new_new_n964__ = p44x1 ? new_new_n962__ : \new_new_mesh8|fx45__ ;
  assign new_new_n966__ = p44y1 ? new_new_n962__ : \new_new_mesh8|fy54__ ;
  assign \new_new_mesh8|fx44__  = ~p44x2 ^ ~new_new_n964__;
  assign \new_new_mesh8|fy44__  = ~p44y2 ^ ~new_new_n966__;
  assign new_new_n970__ = \new_new_mesh8|fx46__  & \new_new_mesh8|fy55__ ;
  assign new_new_n972__ = p45x1 ? new_new_n970__ : \new_new_mesh8|fx46__ ;
  assign new_new_n974__ = p45y1 ? new_new_n970__ : \new_new_mesh8|fy55__ ;
  assign \new_new_mesh8|fx45__  = ~p45x2 ^ ~new_new_n972__;
  assign \new_new_mesh8|fy45__  = ~p45y2 ^ ~new_new_n974__;
  assign new_new_n978__ = \new_new_mesh8|fx47__  & \new_new_mesh8|fy56__ ;
  assign new_new_n980__ = p46x1 ? new_new_n978__ : \new_new_mesh8|fx47__ ;
  assign new_new_n982__ = p46y1 ? new_new_n978__ : \new_new_mesh8|fy56__ ;
  assign \new_new_mesh8|fx46__  = ~p46x2 ^ ~new_new_n980__;
  assign \new_new_mesh8|fy46__  = ~p46y2 ^ ~new_new_n982__;
  assign new_new_n986__ = v04 & \new_new_mesh8|fy57__ ;
  assign new_new_n988__ = p47x1 ? new_new_n986__ : v04;
  assign new_new_n990__ = p47y1 ? new_new_n986__ : \new_new_mesh8|fy57__ ;
  assign \new_new_mesh8|fx47__  = ~p47x2 ^ ~new_new_n988__;
  assign \new_new_mesh8|fy47__  = ~p47y2 ^ ~new_new_n990__;
  assign new_new_n994__ = \new_new_mesh8|fx51__  & \new_new_mesh8|fy60__ ;
  assign new_new_n998__ = p50y1 ? new_new_n994__ : \new_new_mesh8|fy60__ ;
  assign \new_new_mesh8|fy50__  = ~p50y2 ^ ~new_new_n998__;
  assign new_new_n1002__ = \new_new_mesh8|fx52__  & \new_new_mesh8|fy61__ ;
  assign new_new_n1004__ = p51x1 ? new_new_n1002__ : \new_new_mesh8|fx52__ ;
  assign new_new_n1006__ = p51y1 ? new_new_n1002__ : \new_new_mesh8|fy61__ ;
  assign \new_new_mesh8|fx51__  = ~p51x2 ^ ~new_new_n1004__;
  assign \new_new_mesh8|fy51__  = ~p51y2 ^ ~new_new_n1006__;
  assign new_new_n1010__ = \new_new_mesh8|fx53__  & \new_new_mesh8|fy62__ ;
  assign new_new_n1012__ = p52x1 ? new_new_n1010__ : \new_new_mesh8|fx53__ ;
  assign new_new_n1014__ = p52y1 ? new_new_n1010__ : \new_new_mesh8|fy62__ ;
  assign \new_new_mesh8|fx52__  = ~p52x2 ^ ~new_new_n1012__;
  assign \new_new_mesh8|fy52__  = ~p52y2 ^ ~new_new_n1014__;
  assign new_new_n1018__ = \new_new_mesh8|fx54__  & \new_new_mesh8|fy63__ ;
  assign new_new_n1020__ = p53x1 ? new_new_n1018__ : \new_new_mesh8|fx54__ ;
  assign new_new_n1022__ = p53y1 ? new_new_n1018__ : \new_new_mesh8|fy63__ ;
  assign \new_new_mesh8|fx53__  = ~p53x2 ^ ~new_new_n1020__;
  assign \new_new_mesh8|fy53__  = ~p53y2 ^ ~new_new_n1022__;
  assign new_new_n1026__ = \new_new_mesh8|fx55__  & \new_new_mesh8|fy64__ ;
  assign new_new_n1028__ = p54x1 ? new_new_n1026__ : \new_new_mesh8|fx55__ ;
  assign new_new_n1030__ = p54y1 ? new_new_n1026__ : \new_new_mesh8|fy64__ ;
  assign \new_new_mesh8|fx54__  = ~p54x2 ^ ~new_new_n1028__;
  assign \new_new_mesh8|fy54__  = ~p54y2 ^ ~new_new_n1030__;
  assign new_new_n1034__ = \new_new_mesh8|fx56__  & \new_new_mesh8|fy65__ ;
  assign new_new_n1036__ = p55x1 ? new_new_n1034__ : \new_new_mesh8|fx56__ ;
  assign new_new_n1038__ = p55y1 ? new_new_n1034__ : \new_new_mesh8|fy65__ ;
  assign \new_new_mesh8|fx55__  = ~p55x2 ^ ~new_new_n1036__;
  assign \new_new_mesh8|fy55__  = ~p55y2 ^ ~new_new_n1038__;
  assign new_new_n1042__ = \new_new_mesh8|fx57__  & \new_new_mesh8|fy66__ ;
  assign new_new_n1044__ = p56x1 ? new_new_n1042__ : \new_new_mesh8|fx57__ ;
  assign new_new_n1046__ = p56y1 ? new_new_n1042__ : \new_new_mesh8|fy66__ ;
  assign \new_new_mesh8|fx56__  = ~p56x2 ^ ~new_new_n1044__;
  assign \new_new_mesh8|fy56__  = ~p56y2 ^ ~new_new_n1046__;
  assign new_new_n1050__ = v05 & \new_new_mesh8|fy67__ ;
  assign new_new_n1052__ = p57x1 ? new_new_n1050__ : v05;
  assign new_new_n1054__ = p57y1 ? new_new_n1050__ : \new_new_mesh8|fy67__ ;
  assign \new_new_mesh8|fx57__  = ~p57x2 ^ ~new_new_n1052__;
  assign \new_new_mesh8|fy57__  = ~p57y2 ^ ~new_new_n1054__;
  assign new_new_n1058__ = \new_new_mesh8|fx61__  & \new_new_mesh8|fy70__ ;
  assign new_new_n1062__ = p60y1 ? new_new_n1058__ : \new_new_mesh8|fy70__ ;
  assign \new_new_mesh8|fy60__  = ~p60y2 ^ ~new_new_n1062__;
  assign new_new_n1066__ = \new_new_mesh8|fx62__  & \new_new_mesh8|fy71__ ;
  assign new_new_n1068__ = p61x1 ? new_new_n1066__ : \new_new_mesh8|fx62__ ;
  assign new_new_n1070__ = p61y1 ? new_new_n1066__ : \new_new_mesh8|fy71__ ;
  assign \new_new_mesh8|fx61__  = ~p61x2 ^ ~new_new_n1068__;
  assign \new_new_mesh8|fy61__  = ~p61y2 ^ ~new_new_n1070__;
  assign new_new_n1074__ = \new_new_mesh8|fx63__  & \new_new_mesh8|fy72__ ;
  assign new_new_n1076__ = p62x1 ? new_new_n1074__ : \new_new_mesh8|fx63__ ;
  assign new_new_n1078__ = p62y1 ? new_new_n1074__ : \new_new_mesh8|fy72__ ;
  assign \new_new_mesh8|fx62__  = ~p62x2 ^ ~new_new_n1076__;
  assign \new_new_mesh8|fy62__  = ~p62y2 ^ ~new_new_n1078__;
  assign new_new_n1082__ = \new_new_mesh8|fx64__  & \new_new_mesh8|fy73__ ;
  assign new_new_n1084__ = p63x1 ? new_new_n1082__ : \new_new_mesh8|fx64__ ;
  assign new_new_n1086__ = p63y1 ? new_new_n1082__ : \new_new_mesh8|fy73__ ;
  assign \new_new_mesh8|fx63__  = ~p63x2 ^ ~new_new_n1084__;
  assign \new_new_mesh8|fy63__  = ~p63y2 ^ ~new_new_n1086__;
  assign new_new_n1090__ = \new_new_mesh8|fx65__  & \new_new_mesh8|fy74__ ;
  assign new_new_n1092__ = p64x1 ? new_new_n1090__ : \new_new_mesh8|fx65__ ;
  assign new_new_n1094__ = p64y1 ? new_new_n1090__ : \new_new_mesh8|fy74__ ;
  assign \new_new_mesh8|fx64__  = ~p64x2 ^ ~new_new_n1092__;
  assign \new_new_mesh8|fy64__  = ~p64y2 ^ ~new_new_n1094__;
  assign new_new_n1098__ = \new_new_mesh8|fx66__  & \new_new_mesh8|fy75__ ;
  assign new_new_n1100__ = p65x1 ? new_new_n1098__ : \new_new_mesh8|fx66__ ;
  assign new_new_n1102__ = p65y1 ? new_new_n1098__ : \new_new_mesh8|fy75__ ;
  assign \new_new_mesh8|fx65__  = ~p65x2 ^ ~new_new_n1100__;
  assign \new_new_mesh8|fy65__  = ~p65y2 ^ ~new_new_n1102__;
  assign new_new_n1106__ = \new_new_mesh8|fx67__  & \new_new_mesh8|fy76__ ;
  assign new_new_n1108__ = p66x1 ? new_new_n1106__ : \new_new_mesh8|fx67__ ;
  assign new_new_n1110__ = p66y1 ? new_new_n1106__ : \new_new_mesh8|fy76__ ;
  assign \new_new_mesh8|fx66__  = ~p66x2 ^ ~new_new_n1108__;
  assign \new_new_mesh8|fy66__  = ~p66y2 ^ ~new_new_n1110__;
  assign new_new_n1114__ = v06 & \new_new_mesh8|fy77__ ;
  assign new_new_n1116__ = p67x1 ? new_new_n1114__ : v06;
  assign new_new_n1118__ = p67y1 ? new_new_n1114__ : \new_new_mesh8|fy77__ ;
  assign \new_new_mesh8|fx67__  = ~p67x2 ^ ~new_new_n1116__;
  assign \new_new_mesh8|fy67__  = ~p67y2 ^ ~new_new_n1118__;
  assign new_new_n1122__ = \new_new_mesh8|fx71__  & v08;
  assign new_new_n1126__ = p70y1 ? new_new_n1122__ : v08;
  assign \new_new_mesh8|fy70__  = ~p70y2 ^ ~new_new_n1126__;
  assign new_new_n1130__ = \new_new_mesh8|fx72__  & v09;
  assign new_new_n1132__ = p71x1 ? new_new_n1130__ : \new_new_mesh8|fx72__ ;
  assign new_new_n1134__ = p71y1 ? new_new_n1130__ : v09;
  assign \new_new_mesh8|fx71__  = ~p71x2 ^ ~new_new_n1132__;
  assign \new_new_mesh8|fy71__  = ~p71y2 ^ ~new_new_n1134__;
  assign new_new_n1138__ = \new_new_mesh8|fx73__  & v10;
  assign new_new_n1140__ = p72x1 ? new_new_n1138__ : \new_new_mesh8|fx73__ ;
  assign new_new_n1142__ = p72y1 ? new_new_n1138__ : v10;
  assign \new_new_mesh8|fx72__  = ~p72x2 ^ ~new_new_n1140__;
  assign \new_new_mesh8|fy72__  = ~p72y2 ^ ~new_new_n1142__;
  assign new_new_n1146__ = \new_new_mesh8|fx74__  & v11;
  assign new_new_n1148__ = p73x1 ? new_new_n1146__ : \new_new_mesh8|fx74__ ;
  assign new_new_n1150__ = p73y1 ? new_new_n1146__ : v11;
  assign \new_new_mesh8|fx73__  = ~p73x2 ^ ~new_new_n1148__;
  assign \new_new_mesh8|fy73__  = ~p73y2 ^ ~new_new_n1150__;
  assign new_new_n1154__ = \new_new_mesh8|fx75__  & v12;
  assign new_new_n1156__ = p74x1 ? new_new_n1154__ : \new_new_mesh8|fx75__ ;
  assign new_new_n1158__ = p74y1 ? new_new_n1154__ : v12;
  assign \new_new_mesh8|fx74__  = ~p74x2 ^ ~new_new_n1156__;
  assign \new_new_mesh8|fy74__  = ~p74y2 ^ ~new_new_n1158__;
  assign new_new_n1162__ = \new_new_mesh8|fx76__  & v13;
  assign new_new_n1164__ = p75x1 ? new_new_n1162__ : \new_new_mesh8|fx76__ ;
  assign new_new_n1166__ = p75y1 ? new_new_n1162__ : v13;
  assign \new_new_mesh8|fx75__  = ~p75x2 ^ ~new_new_n1164__;
  assign \new_new_mesh8|fy75__  = ~p75y2 ^ ~new_new_n1166__;
  assign new_new_n1170__ = \new_new_mesh8|fx77__  & v14;
  assign new_new_n1172__ = p76x1 ? new_new_n1170__ : \new_new_mesh8|fx77__ ;
  assign new_new_n1174__ = p76y1 ? new_new_n1170__ : v14;
  assign \new_new_mesh8|fx76__  = ~p76x2 ^ ~new_new_n1172__;
  assign \new_new_mesh8|fy76__  = ~p76y2 ^ ~new_new_n1174__;
  assign new_new_n1178__ = v07 & v15;
  assign new_new_n1180__ = p77x1 ? new_new_n1178__ : v07;
  assign new_new_n1182__ = p77y1 ? new_new_n1178__ : v15;
  assign \new_new_mesh8|fx77__  = ~p77x2 ^ ~new_new_n1180__;
  assign \new_new_mesh8|fy77__  = ~p77y2 ^ ~new_new_n1182__;
endmodule


