#pragma once
// LÄCKAGET: köldmedierna. Varje medium är en egen "anläggning" med egen karaktär.
// Värdena är inspirerade av de riktiga ämnena (tryck, glid, brännbarhet, tröghet)
// men inställda för att låta bra, inte för att räkna ut en riktig anläggning.

namespace lackaget {

struct Koldmedium
{
    const char* namn;     // R-nummer, visas i displayen
    const char* sv;       // kort beskrivning
    const char* en;
    float troghet;        // anläggningens tröghet, i taktslag (snabb 0.05 .. seg 1.2)
    float dodtid;         // dödtid, gånger halva börvärdessteget (styr hur den pendlar)
    float forstarkning;   // hur hårt ventilen påverkar trycket
    float dampning;       // hur väl den går att reglera (0 nervös .. 1 lugn)
    float brus;           // givarbrus
    float glid;           // temperaturglid (zeotropa blandningar): börvärdet driver långsamt
    float tryck;          // grundtryck: hur hårt kompressorn mättar
    float ljus;           // förångarens klang (0 mörk .. 1 isig)
    float bubblor;        // benägenhet för flashgas i siktglaset
    float brand;          // brännbarhet: plötsliga tryckspikar
};

inline constexpr Koldmedium kMedier[] = {
    //  namn       sv                                   en                                    trög  död   förs  dämp  brus  glid  tryck ljus  bubb  brand
    { "R134a",   "Lugn och mjuk. Bilens gamla AC.",     "Calm and soft. The old car AC.",      0.50f, 1.0f, 0.8f, 0.80f, 0.06f, 0.0f, 0.35f, 0.45f, 0.10f, 0.0f },
    { "R744",    "CO2. Snabb, nervös, skyhögt tryck.",  "CO2. Fast, nervous, sky-high pressure.",0.08f, 0.6f, 1.4f, 0.35f, 0.22f, 0.0f, 0.90f, 0.75f, 0.30f, 0.0f },
    { "R290",    "Propan. Ljus och explosiv.",          "Propane. Bright and explosive.",      0.20f, 0.8f, 1.2f, 0.50f, 0.10f, 0.0f, 0.60f, 0.85f, 0.15f, 0.7f },
    { "R717",    "Ammoniak. Brutal industri.",          "Ammonia. Brutal industry.",           0.25f, 0.9f, 1.6f, 0.30f, 0.12f, 0.0f, 1.00f, 0.55f, 0.20f, 0.2f },
    { "R404A",   "Frysdisken. Stadig och kall.",        "The freezer aisle. Steady and cold.", 0.30f, 1.0f, 1.0f, 0.60f, 0.08f, 0.10f, 0.55f, 0.50f, 0.20f, 0.0f },
    { "R410A",   "Värmepumpen. Tät och pigg.",          "The heat pump. Tight and lively.",    0.15f, 0.7f, 1.2f, 0.55f, 0.10f, 0.05f, 0.70f, 0.65f, 0.15f, 0.0f },
    { "R32",     "Ny värmepump. Snabb, lite het.",      "New heat pump. Fast, a bit hot.",     0.15f, 0.7f, 1.3f, 0.50f, 0.10f, 0.0f, 0.75f, 0.70f, 0.15f, 0.3f },
    { "R1234yf", "Ny bil-AC. Mjuk med glöd.",           "New car AC. Soft with a glow.",       0.45f, 1.1f, 0.9f, 0.75f, 0.06f, 0.0f, 0.40f, 0.55f, 0.10f, 0.2f },
    { "R600a",   "Isobutan. Kylskåpet hemma, luftigt.", "Isobutane. The fridge at home, airy.",0.35f, 1.2f, 0.9f, 0.70f, 0.08f, 0.0f, 0.30f, 0.80f, 0.12f, 0.6f },
    { "R22",     "Freon. Förbjuden, varm vintage.",     "Freon. Banned, warm vintage.",        0.40f, 1.0f, 1.0f, 0.65f, 0.04f, 0.0f, 0.55f, 0.40f, 0.08f, 0.0f },
    { "R12",     "1960-tal. Mörk, rund, långsam.",      "The 1960s. Dark, round, slow.",       0.60f, 1.2f, 0.7f, 0.80f, 0.03f, 0.0f, 0.40f, 0.30f, 0.05f, 0.0f },
    { "R407C",   "Stort glid. Driver hela tiden.",      "Big glide. Always drifting.",         0.35f, 1.0f, 1.0f, 0.60f, 0.08f, 0.60f, 0.50f, 0.50f, 0.20f, 0.0f },
    { "R449A",   "Butikskyla. Glid och tryck.",         "Supermarket cooling. Glide and push.",0.30f, 1.0f, 1.0f, 0.60f, 0.08f, 0.35f, 0.55f, 0.55f, 0.20f, 0.0f },
    { "R513A",   "Vattenkylaggregat. Mjukt brett.",     "Water chiller. Soft and wide.",       0.45f, 1.0f, 0.9f, 0.75f, 0.06f, 0.0f, 0.40f, 0.50f, 0.10f, 0.0f },
    { "R1270",   "Propylen. Vass, het, farlig.",        "Propylene. Sharp, hot, dangerous.",   0.18f, 0.8f, 1.3f, 0.45f, 0.12f, 0.0f, 0.65f, 0.90f, 0.18f, 0.8f },
    { "R23",     "Kaskad -80. Isig och skör.",          "Cascade -80. Icy and brittle.",       0.10f, 0.5f, 1.5f, 0.30f, 0.25f, 0.0f, 0.80f, 1.00f, 0.40f, 0.0f },
    { "R507A",   "Kylrum. Tung och jämn.",              "Cold room. Heavy and even.",          0.32f, 1.0f, 1.0f, 0.62f, 0.07f, 0.02f, 0.58f, 0.45f, 0.18f, 0.0f },
    { "R718",    "Vatten. Segt, djupt, kokar.",         "Water. Slow, deep, boiling.",         1.20f, 1.5f, 0.6f, 0.90f, 0.03f, 0.0f, 0.25f, 0.25f, 0.30f, 0.0f },
    { "R13B1",   "Halon. Glömd, kall, metallisk.",      "Halon. Forgotten, cold, metallic.",   0.22f, 0.7f, 1.2f, 0.45f, 0.14f, 0.0f, 0.70f, 0.95f, 0.25f, 0.0f },
    { "R170",    "Etan. Ultrakall och spetsig.",        "Ethane. Ultra-cold and spiky.",       0.12f, 0.6f, 1.4f, 0.35f, 0.18f, 0.0f, 0.75f, 0.92f, 0.30f, 0.9f },
};

inline constexpr int kNumMedier = (int) (sizeof (kMedier) / sizeof (kMedier[0]));

} // namespace lackaget
