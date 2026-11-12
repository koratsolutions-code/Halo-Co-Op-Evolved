#pragma once
// Skins de los jugadores: cada uno puede verse como otro personaje de Halo CE, pero sigue siendo Master Chief en
// todo lo demás (hitbox, velocidad, salto, física). Solo existen en las misiones que cargan a ese personaje; si la
// elegida no está, el jugador sale como Master Chief. Común al host, al cliente y al mod del juego.

// De dónde salen las animaciones de cada skin. Una malla solo se puede animar con animaciones hechas para su
// mismo esqueleto (la malla va pegada a sus huesos por número, en datos que el juego ya subió a la tarjeta
// gráfica), y esas animaciones tienen que traer todo lo que hace un jugador (cada arma, granadas, vehículos).
enum class SkinAnimations {
    own,     // las del propio personaje (completas: marine, Elite, Flood)
    marine,  // las del marine: solo si el esqueleto del modelo es idéntico al del marine (misma suma de nodos)
};

struct Skin {
    const char* name_en;
    const char* name_es;
    const char* tag;  // biped de la misión con el modelo
    SkinAnimations animations;
};

// El piloto no está: con sus animaciones el juego se cierra (no traen las de las armas) y su esqueleto tiene un
// hueso más que el del marine ("frame visor"), así que las del marine tampoco le calzan.
inline constexpr Skin k_skins[] = {
    {"Master Chief", "Master Chief", "characters\\cyborg\\cyborg", SkinAnimations::own},
    {"Marine", "Marine", "characters\\marine\\marine", SkinAnimations::own},
    {"Armored marine", "Marine con armadura", "characters\\marine_armored\\marine_armored", SkinAnimations::own},
    {"Crewman", "Tripulante", "characters\\crewman\\crewman", SkinAnimations::marine},
    {"Captain Keyes (fatigues)", "Capitán Keyes (campaña)", "characters\\captain_ingame\\captain_ingame",
     SkinAnimations::marine},
    {"Captain Keyes (dress)", "Capitán Keyes (gala)", "characters\\captain\\captain", SkinAnimations::marine},
    {"Jenkins", "Jenkins", "characters\\marine_cinematic\\jenkins\\jenkins", SkinAnimations::marine},
    {"Flood marine", "Marine Flood", "characters\\floodcombat_human\\floodcombat_human", SkinAnimations::own},
    {"Elite", "Elite", "characters\\elite\\elite", SkinAnimations::own},
};
inline constexpr int k_skin_count = int(sizeof(k_skins) / sizeof(k_skins[0]));
static_assert(k_skin_count <= 16, "skins_available es una máscara de 16 bits");
