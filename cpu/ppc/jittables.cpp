#include <cinttypes>
#include <map>
#include "jittables.h"
#include "ppcdefs.h"

uint16_t main_index_tab[64] = {
    PPCInstr::illegal,   PPCInstr::illegal,   PPCInstr::illegal,
    PPCInstr::twi,       PPCInstr::illegal,   PPCInstr::illegal,
    PPCInstr::illegal,   PPCInstr::mulli,     PPCInstr::subfic,
    PPCInstr::dozi,      PPCInstr::cmpli,     PPCInstr::cmpi,
    PPCInstr::addic,     PPCInstr::addicdot,  PPCInstr::addi,
    PPCInstr::addis,     PPCInstr::illegal,   PPCInstr::sc,
    PPCInstr::illegal,   PPCInstr::illegal,   PPCInstr::rlwimix,
    PPCInstr::rlwinmx,   PPCInstr::rlmi,      PPCInstr::rlwnmx,
    PPCInstr::ori,       PPCInstr::oris,      PPCInstr::xori,
    PPCInstr::xoris,     PPCInstr::andidot,   PPCInstr::andisdot,
    PPCInstr::illegal,   PPCInstr::illegal,   PPCInstr::lwz,
    PPCInstr::lwzu,      PPCInstr::lbz,       PPCInstr::lbzu,
    PPCInstr::stw,       PPCInstr::stwu,      PPCInstr::stb,
    PPCInstr::stbu,      PPCInstr::lhz,       PPCInstr::lhzu,
    PPCInstr::lha,       PPCInstr::lhau,      PPCInstr::sth,
    PPCInstr::sthu,      PPCInstr::lmw,       PPCInstr::stmw,
    PPCInstr::lfs,       PPCInstr::lfsu,      PPCInstr::lfd,
    PPCInstr::lfdu,      PPCInstr::stfs,      PPCInstr::stfsu,
    PPCInstr::stfd,      PPCInstr::stfdu,     PPCInstr::illegal,
    PPCInstr::illegal,   PPCInstr::illegal,   PPCInstr::illegal,
    PPCInstr::illegal,   PPCInstr::illegal,   PPCInstr::illegal,
    PPCInstr::illegal
};

uint16_t subgrp16_index_tab[4] = {
    PPCInstr::bc, PPCInstr::bcl, PPCInstr::bca, PPCInstr::bcla
};

uint16_t subgrp18_index_tab[4] = {
    PPCInstr::b, PPCInstr::bl, PPCInstr::ba, PPCInstr::bla
};

uint16_t subgrp19_index_tab[2048] = { PPCInstr::illegal };

uint16_t subgrp31_index_tab[2048] = { PPCInstr::illegal };

uint16_t subgrp59_index_tab[64]   = { PPCInstr::illegal };

uint16_t subgrp63_index_tab[2048] = { PPCInstr::illegal };

static std::map<uint16_t, PPCInstr> Subop19Init = {
    {   32, PPCInstr::bclr   },
    {   33, PPCInstr::bclrl  },
    {   66, PPCInstr::crnor  },
    {  100, PPCInstr::rfi    },
    {  258, PPCInstr::crandc },
    {  300, PPCInstr::isync  },
    {  386, PPCInstr::crxor  },
    {  450, PPCInstr::crnand },
    {  514, PPCInstr::crand  },
    {  578, PPCInstr::creqv  },
    {  834, PPCInstr::crorc  },
    {  898, PPCInstr::cror   },
    { 1056, PPCInstr::bcctr  },
    { 1057, PPCInstr::bcctrl }
};

static std::map<uint16_t, PPCInstr> Subop31Init = {
    {    0, PPCInstr::cmp},         {    8, PPCInstr::tw},
    {   16, PPCInstr::subfc},       {   17, PPCInstr::subfcdot},
    {   20, PPCInstr::addc},        {   21, PPCInstr::addcdot},
    {   22, PPCInstr::mulhwu},      {   23, PPCInstr::mulhwudot},
    {   38, PPCInstr::mfcr},        {   40, PPCInstr::lwarx},
    {   46, PPCInstr::lwzx},        {   48, PPCInstr::slw},
    {   49, PPCInstr::slwdot},      {   52, PPCInstr::cntlzw},
    {   53, PPCInstr::cntlzwdot},   {   56, PPCInstr::ppc_and},
    {   57, PPCInstr::anddot},      {   58, PPCInstr::maskg},
    {   59, PPCInstr::maskgdot},    {   64, PPCInstr::cmpl},
    {   80, PPCInstr::subf},        {   81, PPCInstr::subfdot},
    {  108, PPCInstr::dcbst},       {  110, PPCInstr::lwzux},
    {  120, PPCInstr::andc},        {  121, PPCInstr::andcdot},
    {  150, PPCInstr::mulhw},       {  151, PPCInstr::mulhwdot},
    {  166, PPCInstr::mfmsr},       {  172, PPCInstr::dcbf},
    {  174, PPCInstr::lbzx},        {  208, PPCInstr::neg},
    {  209, PPCInstr::negdot},      {  214, PPCInstr::mul},
    {  215, PPCInstr::muldot},      {  238, PPCInstr::lbzux},
    {  248, PPCInstr::nor},         {  249, PPCInstr::nordot},
    {  272, PPCInstr::subfe},       {  273, PPCInstr::subfedot},
    {  276, PPCInstr::adde},        {  277, PPCInstr::addedot},
    {  288, PPCInstr::mtcrf},       {  292, PPCInstr::mtmsr},
    {  301, PPCInstr::stwcx},       {  302, PPCInstr::stwx},
    {  304, PPCInstr::slq},         {  305, PPCInstr::slqdot},
    {  306, PPCInstr::sle},         {  307, PPCInstr::sledot},
    {  366, PPCInstr::stwux},       {  368, PPCInstr::sliq},
    {  400, PPCInstr::subfze},      {  401, PPCInstr::subfzedot},
    {  404, PPCInstr::addze},       {  405, PPCInstr::addzedot},
    {  420, PPCInstr::mtsr},        {  430, PPCInstr::stbx},
    {  432, PPCInstr::sllq},        {  433, PPCInstr::sllqdot},
    {  434, PPCInstr::sleq},        {  436, PPCInstr::sleqdot},
    {  464, PPCInstr::subfme},      {  465, PPCInstr::subfmedot},
    {  468, PPCInstr::addme},       {  469, PPCInstr::addmedot},
    {  470, PPCInstr::mullw},       {  471, PPCInstr::mullwdot},
    {  484, PPCInstr::mtsrin},      {  492, PPCInstr::dcbtst},
    {  494, PPCInstr::stbux},       {  496, PPCInstr::slliq},
    {  497, PPCInstr::slliqdot},    {  528, PPCInstr::doz},
    {  529, PPCInstr::dozdot},      {  532, PPCInstr::add},
    {  533, PPCInstr::adddot},      {  554, PPCInstr::lscbx},
    {  555, PPCInstr::lscbxdot},    {  556, PPCInstr::dcbt},
    {  558, PPCInstr::lhzx},        {  568, PPCInstr::eqv},
    {  569, PPCInstr::eqvdot},      {  612, PPCInstr::tlbie},
    {  622, PPCInstr::lhzux},       {  632, PPCInstr::ppc_xor},
    {  633, PPCInstr::xordot},      {  662, PPCInstr::div},
    {  663, PPCInstr::divdot},      {  678, PPCInstr::mfspr},
    {  686, PPCInstr::lhax},        {  720, PPCInstr::abs},
    {  721, PPCInstr::absdot},      {  726, PPCInstr::divs},
    {  727, PPCInstr::divsdot},     {  740, PPCInstr::tlbia},
    {  742, PPCInstr::mftb},        {  750, PPCInstr::lhaux},
    {  814, PPCInstr::sthx},        {  824, PPCInstr::orc},
    {  825, PPCInstr::orcdot},      {  878, PPCInstr::sthux},
    {  888, PPCInstr::ppc_or},      {  889, PPCInstr::ordot},
    {  918, PPCInstr::divwu},       {  919, PPCInstr::divwudot},
    {  934, PPCInstr::mtspr},       {  940, PPCInstr::dcbi},
    {  952, PPCInstr::nand},        {  953, PPCInstr::nanddot},
    {  976, PPCInstr::nabs},        {  977, PPCInstr::nabsdot},
    {  982, PPCInstr::divw},        {  983, PPCInstr::divwdot},
    { 1024, PPCInstr::mcrxr},       { 1040, PPCInstr::subfco},
    { 1041, PPCInstr::subfcodot},   { 1044, PPCInstr::addco},
    { 1045, PPCInstr::addcodot},    { 1062, PPCInstr::clcs},
    { 1063, PPCInstr::clcsdot},     { 1066, PPCInstr::lswx},
    { 1068, PPCInstr::lwbrx},       { 1070, PPCInstr::lfsx},
    { 1072, PPCInstr::srw},         { 1073, PPCInstr::srwdot},
    { 1074, PPCInstr::rrib},        { 1075, PPCInstr::rribdot},
    { 1082, PPCInstr::maskir},      { 1083, PPCInstr::maskirdot},
    { 1104, PPCInstr::subfo},       { 1105, PPCInstr::subfodot},
    { 1132, PPCInstr::tlbsync},     { 1134, PPCInstr::lfsux},
    { 1190, PPCInstr::mfsr},        { 1194, PPCInstr::lswi},
    { 1196, PPCInstr::sync},        { 1198, PPCInstr::lfdx},
    { 1232, PPCInstr::nego},        { 1233, PPCInstr::negodot},
    { 1238, PPCInstr::mulo},        { 1239, PPCInstr::mulodot},
    { 1262, PPCInstr::lfdux},       { 1296, PPCInstr::subfeo},
    { 1297, PPCInstr::subfeodot},   { 1300, PPCInstr::addeo},
    { 1301, PPCInstr::addeodot},    { 1318, PPCInstr::mfsrin},
    { 1322, PPCInstr::stswx},       { 1324, PPCInstr::stwbrx},
    { 1326, PPCInstr::stfsx},       { 1328, PPCInstr::srq},
    { 1329, PPCInstr::srqdot},      { 1330, PPCInstr::sre},
    { 1331, PPCInstr::sredot},      { 1390, PPCInstr::stfsux},
    { 1392, PPCInstr::sriq},        { 1393, PPCInstr::sriqdot},
    { 1424, PPCInstr::subfzeo},     { 1425, PPCInstr::subfzeodot},
    { 1428, PPCInstr::addzeo},      { 1429, PPCInstr::addzeodot},
    { 1450, PPCInstr::stswi},       { 1454, PPCInstr::stfdx},
    { 1456, PPCInstr::srlq},        { 1457, PPCInstr::srlqdot},
    { 1458, PPCInstr::sreq     },   { 1459, PPCInstr::sreqdot},
    { 1488, PPCInstr::subfmeo  },   { 1489, PPCInstr::subfmeodot},
    { 1492, PPCInstr::addmeo   },   { 1493, PPCInstr::addmeodot},
    { 1494, PPCInstr::mullwo   },   { 1495, PPCInstr::mullwodot},
    { 1518, PPCInstr::stfdux   },   { 1520, PPCInstr::srliq},
    { 1521, PPCInstr::srliqdot },   { 1552, PPCInstr::dozo},
    { 1553, PPCInstr::dozodot  },   { 1556, PPCInstr::addo},
    { 1557, PPCInstr::addodot  },   { 1580, PPCInstr::lhbrx},
    { 1584, PPCInstr::sraw     },   { 1585, PPCInstr::srawdot},
    { 1648, PPCInstr::srawi    },   { 1649, PPCInstr::srawidot},
    { 1686, PPCInstr::divo     },   { 1687, PPCInstr::divodot},
    { 1708, PPCInstr::eieio    },   { 1744, PPCInstr::abso},
    { 1745, PPCInstr::absodot  },   { 1750, PPCInstr::divso},
    { 1751, PPCInstr::divsodot },   { 1836, PPCInstr::sthbrx},
    { 1840, PPCInstr::sraq     },   { 1841, PPCInstr::sraqdot},
    { 1842, PPCInstr::srea     },   { 1843, PPCInstr::sreadot},
    { 1844, PPCInstr::extsh    },   { 1845, PPCInstr::extshdot},
    { 1904, PPCInstr::sraiq    },   { 1905, PPCInstr::sraiqdot},
    { 1908, PPCInstr::extsb    },   { 1909, PPCInstr::extsbdot},
    { 1942, PPCInstr::divwuo   },   { 1943, PPCInstr::divwuodot},
    { 1956, PPCInstr::tlbld    },   { 1964, PPCInstr::icbi},
    { 1966, PPCInstr::stfiwx   },   { 2000, PPCInstr::nabso},
    { 2001, PPCInstr::nabsodot },   { 2006, PPCInstr::divwo},
    { 2007, PPCInstr::divwodot },   { 2020, PPCInstr::tlbli},
    { 2028, PPCInstr::dcbz     }
};

void init_jit_tables() {
    std::map<uint16_t, PPCInstr>::iterator it;

    for (it = Subop19Init.begin(); it != Subop19Init.end(); it++ ) {
        subgrp19_index_tab[it->first] = it->second;
    }

    for (it = Subop31Init.begin(); it != Subop31Init.end(); it++ ) {
        subgrp31_index_tab[it->first] = it->second;
    }
}
