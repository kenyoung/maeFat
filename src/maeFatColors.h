/*
  This file defines the colors used by the maeFat program.
 */
#define N_COLORS        (19)

#define MAX16        (65535)
#define DOUBLE_MAX16 (65635.0)
#define OR_BLACK         (0)
#define OR_WHITE         (1)
#define OR_BLUE          (2)
#define OR_ORANGE        (3)
#define OR_GREY          (4)
#define OR_DARK_GREY     (5)
#define OR_GREEN         (6)
#define OR_DARK_GREEN    (7)
#define OR_RED           (8)
#define OR_EQUATOR       (9)
#define OR_YELLOW       (10)
#define OR_LIGHT_YELLOW (11)
#define OR_CREAM        (12)
#define OR_DARK_CREAM   (13)
#define OR_BLUE_GREEN   (14)
#define OR_YELLOW_GREEN (15)
#define OR_PINK         (16)
#define OR_LIGHT_BLUE   (17)
#define OR_FAINT_YELLOW (18)

unsigned short orreryColorRGB[N_COLORS][3] =
  {{    0,     0,     0}, /* Black        */
   {MAX16, MAX16, MAX16}, /* White        */
   {20000, 20000, MAX16}, /* Blue         */
   {MAX16, 32767,     0}, /* Orange       */
   {41000, 41000, 41000}, /* Grey         */
   {20000, 18000, 20000}, /* Dark Grey    */
   {    0, MAX16,     0}, /* Green        */
   {    0, 40000,     0}, /* Dark Green   */
   {MAX16,     0,     0}, /* Red          */
   {MAX16, MAX16, 32767}, /* Equator      */
   {MAX16, MAX16,     0}, /* Yellow       */
   {MAX16, MAX16, 32000}, /* Light Yellow */
   {MAX16, MAX16, 40000}, /* Cream        */
   {39321, 39321, 24000}, /* Dark Cream   */
   {    0, 20000, 20000}, /* Blue Green   */
   {43000, MAX16,     0}, /* Yellow Green */
   {MAX16, 25000, 35000}, /* Pink         */
   {42000, MAX16, MAX16}, /* Light_Blue   */
   {52000, 52000, 35000}  /* Faint Yellow */
  };
