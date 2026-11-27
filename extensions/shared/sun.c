/* SPDX-License-Identifier: GPL-3.0-only */

/* Sunset and sunrise for the city of the iPod's time zone. Apple's zone records
 * have no coordinates, so each has one here, by its place in TimeZones. The
 * maths is NOAA's low-accuracy solar position (declination and equation of time
 * from the fractional year) in fixed point, a minute or two from the full
 * algorithm away from the poles. Angles are turns of 65536, fractions Q14. */

#include "sun.h"
#include "osos-2.0.5.h"
#include <stdint.h>

/* Latitude and longitude in hundredths of a degree, north and east positive,
 * in TimeZones order. */
static const int16_t places[TimeZone_Count][2] = {
    {5188, -17666},  /* Adak */
    {2131, -15786},  /* Honolulu */
    {6122, -14990},  /* Anchorage */
    {3732, -12203},  /* California */
    {3732, -12203},  /* Cupertino */
    {3405, -11824},  /* Los Angeles */
    {4552, -12268},  /* Portland */
    {3272, -11716},  /* San Diego */
    {3777, -12242},  /* San Francisco */
    {4761, -12233},  /* Seattle */
    {4928, -12312},  /* Vancouver */
    {5105, -11407},  /* Calgary */
    {3974, -10499},  /* Denver */
    {3345, -11207},  /* Phoenix */
    {4076, -11189},  /* Salt Lake City */
    {1211, -8624},   /* Managua */
    {4159, -9362},   /* Des Moines */
    {3027, -9774},   /* Austin */
    {4188, -8763},   /* Chicago */
    {3278, -9680},   /* Dallas */
    {1463, -9051},   /* Guatemala */
    {3515, -9005},   /* Memphis */
    {1943, -9913},   /* Mexico City */
    {4498, -9327},   /* Minneapolis */
    {5045, -10462},  /* Regina */
    {993, -8408},    /* San José */
    {1369, -8922},   /* San Salvador */
    {3863, -9020},   /* St. Louis */
    {1407, -8719},   /* Tegucigalpa */
    {4990, -9714},   /* Winnipeg */
    {1854, -7234},   /* Port-au-Prince */
    {3375, -8439},   /* Atlanta */
    {471, -7407},    /* Bogotá */
    {4236, -7106},   /* Boston */
    {3996, -8300},   /* Columbus */
    {4233, -8305},   /* Detroit */
    {2311, -8237},   /* Havana */
    {3977, -8616},   /* Indianapolis */
    {3596, -8392},   /* Knoxville */
    {-1205, -7704},  /* Lima */
    {4299, -7146},   /* Manchester */
    {2576, -8019},   /* Miami */
    {4550, -7357},   /* Montréal */
    {4071, -7401},   /* New York */
    {4542, -7570},   /* Ottawa */
    {898, -7952},    /* Panama */
    {3995, -7517},   /* Philadelphia */
    {-18, -7847},    /* Quito */
    {4365, -7938},   /* Toronto */
    {3891, -7704},   /* Washington, D.C. */
    {-2526, -5758},  /* Asunción */
    {1048, -6690},   /* Caracas */
    {680, -5816},    /* Georgetown */
    {4465, -6358},   /* Halifax */
    {-1650, -6815},  /* La Paz */
    {1847, -6611},   /* San Juan */
    {-3345, -7067},  /* Santiago */
    {1849, -6993},   /* Santo Domingo */
    {4756, -5271},   /* St. John's */
    {-1579, -4788},  /* Brasília */
    {-3460, -5838},  /* Buenos Aires */
    {492, -5231},    /* Cayenne */
    {-3490, -5616},  /* Montevideo */
    {6418, -5172},   /* Nuuk */
    {585, -5520},    /* Paramaribo */
    {-805, -3488},   /* Recife */
    {-2291, -4317},  /* Rio de Janeiro */
    {-2355, -4663},  /* São Paulo */
    {-5428, -3651},  /* Grytviken */
    {3774, -2567},   /* Ponta Delgada */
    {5190, -847},    /* Cork */
    {560, -19},      /* Accra */
    {1264, -800},    /* Bamako */
    {5148, -318},    /* Cardiff */
    {964, -1358},    /* Conakry */
    {1472, -1747},   /* Dakar */
    {5335, -626},    /* Dublin */
    {5595, -319},    /* Edinburgh */
    {848, -1323},    /* Freetown */
    {3872, -914},    /* Lisbon */
    {5151, -13},     /* London */
    {630, -1080},    /* Monrovia */
    {1808, -1598},   /* Nouakchott */
    {1237, -152},    /* Ouagadougou */
    {6415, -2194},   /* Reykjavík */
    {3402, -683},    /* Rabat */
    {3675, 306},     /* Algiers */
    {5237, 490},     /* Amsterdam */
    {439, 1856},     /* Bangui */
    {4479, 2045},    /* Belgrade */
    {5252, 1340},    /* Berlin */
    {5085, 435},     /* Brussels */
    {4750, 1904},    /* Budapest */
    {5568, 1257},    /* Copenhagen */
    {405, 977},      /* Douala */
    {4620, 614},     /* Geneva */
    {-444, 1527},    /* Kinshasa */
    {652, 338},      /* Lagos */
    {4606, 1451},    /* Ljubljana */
    {-884, 1323},    /* Luanda */
    {4042, -370},    /* Madrid */
    {4814, 1158},    /* Munich */
    {1213, 1506},    /* Ndjamena */
    {5991, 1075},    /* Oslo */
    {4886, 235},     /* Paris */
    {5008, 1444},    /* Prague */
    {4190, 1250},    /* Roma (Rome) */
    {5933, 1807},    /* Stockholm */
    {3681, 1018},    /* Tunis */
    {4821, 1637},    /* Vienna */
    {5223, 2101},    /* Warsaw */
    {4581, 1598},    /* Zagreb */
    {4738, 854},     /* Zurich */
    {3289, 1319},    /* Tripoli */
    {3195, 3593},    /* Amman */
    {3993, 3286},    /* Ankara */
    {3389, 3550},    /* Beirut */
    {4443, 2610},    /* Bucharest */
    {3004, 3124},    /* Cairo */
    {-3392, 1842},   /* Cape Town */
    {-1783, 3105},   /* Harare */
    {6017, 2494},    /* Helsinki */
    {4101, 2898},    /* Istanbul */
    {3177, 3521},    /* Jerusalem */
    {5045, 3052},    /* Kiev */
    {-1539, 2832},   /* Lusaka */
    {-2597, 3257},   /* Maputo */
    {4270, 2332},    /* Sofia */
    {3351, 3628},    /* Damascus */
    {3798, 2373},    /* Athens */
    {-1888, 4751},   /* Antananarivo */
    {1550, 3256},    /* Khartoum */
    {903, 3874},     /* Addis Ababa */
    {1532, 3893},    /* Asmera */
    {3331, 4436},    /* Baghdad */
    {-679, 3921},    /* Dar es Salaam */
    {1159, 4315},    /* Djibouti */
    {2529, 5153},    /* Doha */
    {35, 3258},      /* Kampala */
    {2623, 5059},    /* Manama */
    {2139, 3986},    /* Mecca */
    {205, 4532},     /* Mogadishu */
    {5576, 3762},    /* Moscow */
    {-129, 3682},    /* Nairobi */
    {2471, 4668},    /* Riyadh */
    {1537, 4419},    /* Sanaa */
    {5993, 3034},    /* St. Petersburg */
    {2938, 4799},    /* Kuwait */
    {3569, 5139},    /* Tehran */
    {2445, 5438},    /* Abu Dhabi */
    {2359, 5841},    /* Muscat */
    {-2016, 5750},   /* Port Louis */
    {-462, 5545},    /* Victoria */
    {3456, 6921},    /* Kabul */
    {4871, 4451},    /* Volgograd */
    {3368, 7305},    /* Islamabad */
    {418, 7351},     /* Male */
    {4130, 6924},    /* Tashkent */
    {5684, 6061},    /* Yekaterinburg */
    {1308, 8027},    /* Chennai (Madras) */
    {693, 7986},     /* Colombo */
    {2257, 8836},    /* Kolkata (Calcutta) */
    {1908, 7288},    /* Mumbai (Bombay) */
    {2861, 7721},    /* New Delhi */
    {2772, 8532},    /* Katmandu */
    {2381, 9041},    /* Dhaka */
    {5503, 8292},    /* Novosibirsk */
    {1687, 9620},    /* Rangoon */
    {1376, 10050},   /* Bangkok */
    {5499, 7337},    /* Omsk */
    {4789, 10691},   /* Ulaanbaatar */
    {1082, 10663},   /* Thanh Pho Ho Chi Minh */
    {2103, 10585},   /* Hanoi */
    {-621, 10685},   /* Jakarta */
    {5601, 9289},    /* Krasnoyarsk */
    {1156, 10493},   /* Phnom Penh */
    {3990, 11641},   /* Beijing */
    {2313, 11326},   /* Canton */
    {2232, 11417},   /* Hong Kong */
    {314, 10169},    /* Kuala Lumpur */
    {1460, 12098},   /* Manila */
    {-3195, 11586},  /* Perth */
    {3123, 12147},   /* Shanghai */
    {135, 10382},    /* Singapore */
    {2503, 12157},   /* Taipei */
    {3934, 11736},   /* Tientsin */
    {3469, 13550},   /* Osaka */
    {3904, 12576},   /* Pyongyang */
    {3757, 12698},   /* Seoul */
    {3568, 13969},   /* Tokyo */
    {6203, 12973},   /* Yakutsk */
    {-1246, 13084},  /* Darwin */
    {-3493, 13860},  /* Adelaide */
    {-2747, 15303},  /* Brisbane */
    {-3528, 14913},  /* Canberra */
    {1344, 14479},   /* Guam */
    {-4288, 14733},  /* Hobart */
    {-3781, 14496},  /* Melbourne */
    {-3387, 15121},  /* Sydney */
    {4312, 13189},   /* Vladivostok */
    {5956, 15080},   /* Magadan */
    {-2226, 16646},  /* Nouméa */
    {6473, 17751},   /* Anadyr */
    {-4129, 17478},  /* Wellington */
    {-1428, -17070}, /* Pago Pago */
};

/* sin over a quarter turn in 64 steps, Q14. */
static const uint16_t quarter[65] = {
    0,     402,   804,   1205,  1606,  2006,  2404,  2801,  3196,  3590,  3981,
    4370,  4756,  5139,  5520,  5897,  6270,  6639,  7005,  7366,  7723,  8076,
    8423,  8765,  9102,  9434,  9760,  10080, 10394, 10702, 11003, 11297, 11585,
    11866, 12140, 12406, 12665, 12916, 13160, 13395, 13623, 13842, 14053, 14256,
    14449, 14635, 14811, 14978, 15137, 15286, 15426, 15557, 15679, 15791, 15893,
    15986, 16069, 16143, 16207, 16261, 16305, 16340, 16364, 16379, 16384,
};

static int32_t sine(uint32_t angle)
{
    uint32_t a = angle & 0x7fff;
    if (a > 0x4000)
        a = 0x8000 - a;
    uint32_t i = a >> 8, f = a & 0xff;
    int32_t v =
        i == 64 ? quarter[64] : quarter[i] + (((quarter[i + 1] - quarter[i]) * f) >> 8);
    return angle & 0x8000 ? -v : v;
}

static int32_t cosine(uint32_t angle)
{
    return sine(angle + 0x4000);
}

/* Hundredths of a degree to turns. */
static uint32_t turns(int32_t centidegrees)
{
    return (uint32_t)(centidegrees * 65536 / 36000);
}

int sun_place(uint32_t zone_id)
{
    for (int i = 0; i < TimeZone_Count; i++) {
        const uint8_t *zone = TimeZones + i * TimeZone_bytes + TimeZone_zone_id;
        if ((uint32_t)(zone[0] | zone[1] << 8) == zone_id)
            return i;
    }
    return -1;
}

int sun_times(int place, int day_of_year, int *rise, int *set)
{
    int32_t latitude = places[place][0], longitude = places[place][1];
    uint32_t year = (uint32_t)((day_of_year - 1) * 65536 / 365);
    int32_t c1 = cosine(year), s1 = sine(year), c2 = cosine(2 * year),
            s2 = sine(2 * year);
    int32_t c3 = cosine(3 * year), s3 = sine(3 * year);
    /* Declination in turns, from NOAA's coefficients in radians times 65536/2pi. */
    int32_t declination =
        72 + ((-4171 * c1 + 733 * s1 - 70 * c2 + 9 * s2 - 28 * c3 + 15 * s3) >> 14);
    /* Equation of time in hundredths of a minute. */
    int32_t equation = 2 + ((43 * c1 - 735 * s1 - 335 * c2 - 936 * s2) >> 14);
    int32_t sin_lat = sine(turns(latitude)), cos_lat = cosine(turns(latitude));
    int32_t sin_dec = sine((uint32_t)declination),
            cos_dec = cosine((uint32_t)declination);
    /* cos of the hour angle at which the sun's centre is 0.833 degrees below
     * the horizon (refraction and its radius): cos(90.833) is -238 in Q14. */
    int32_t across = (cos_lat * cos_dec) >> 14;
    int32_t numerator = -238 * 16384 - sin_lat * sin_dec;
    if (across <= 0 || numerator >= across * 16384)
        return SUN_NEVER_RISES;
    if (numerator <= -across * 16384)
        return SUN_NEVER_SETS;
    int32_t cos_hour = numerator / across;
    /* cos falls over the first half turn: the largest angle whose cos is above. */
    uint32_t low = 0, high = 0x8000;
    while (high - low > 1) {
        uint32_t middle = (low + high) / 2;
        if (cosine(middle) > cos_hour)
            low = middle;
        else
            high = middle;
    }
    int32_t hour = (int32_t)(low * 45 >> 11); /* minutes: 1440 per turn */
    int32_t noon = 720 - longitude / 25 - equation / 100;
    *rise = noon - hour;
    *set = noon + hour;
    return SUN_RISES;
}
