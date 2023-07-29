/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "max7219.h"
#include "zusi3.h"
#include "z3_pzblzb.h"
#include "z3_tueren.h"
#include "z3_sifa.h"


typedef struct {
    MaxLed* lm;
    z3_led_status* src;
    bool* takt;
} leuchtmelder_t;

#define ANZ_LM  9
leuchtmelder_t alle_lm[ANZ_LM];

ZusiClient* zusi;
//Das Ethernet-Interface
EthernetInterface eth;
//Den Max7219 an SPI1 initialisieren
MaxChip max1(PB_5, PB_3, PA_10);
MaxChip max2(PC_3, PD_3, PD_4);
MaxChip max3(PC_12, PC_10, PD_2);
//Einen Leuchtmelder für die Statusmeldung reservieren
MaxLed lm_stoer(&max1, DIG3, SEGF);

//Globaler Blink-Takt
Thread blinken;
bool blink500, blink1000;

void on_data_arrive(word key, word id)
{
    ;
}

void blink_takt(){
    byte t5 = 0;
    byte t10 = 0;

    while(true) {
        ThisThread::sleep_for(100ms);
        t5++;
        t10++;
        if (t5 == 5) {
            t5 = 0;
            blink500 = !blink500;
        }
        if (t10 == 10) {
            t10 = 0;
            blink1000 = !blink1000;
        }
    }
}

void leuchtmelder_update() {

    switch(zusi->get_status()) {
        case status_closed:
            lm_stoer.write(true);
            break;
        case status_connecting:
            lm_stoer.write(blink1000);
            break;
        case status_faulty:
            lm_stoer.write(blink500);
            break;
        case status_online:
            lm_stoer.write(false);
            break;
    }

    for (byte n = 0; n <= ANZ_LM -1; n++) {
        switch(*alle_lm[n].src) {
        case z3_off:
        case z3_off_forced:
            alle_lm[n].lm->write(false);
            break;
        case z3_on:
            alle_lm[n].lm->write(true);
            break;
        case z3_flash:
            alle_lm[n].lm->write(*alle_lm[n].takt);
            break;
        case z3_alternate:
            alle_lm[n].lm->write(!*alle_lm[n].takt);
            break;
        }
    }
}

int main()
{

    float v = 0.0;
    //Den TCP Client mit den ID-Daten und Callback Funktion initialisieren
    zusi = new ZusiClient("Pseudo MFA20", "1.0", &on_data_arrive);

    //Struktur für PZB-Melder
    zusi_pzb_data pzb = { z3_off }; //Struktur für Datenempfang
    MaxLed lm_pzb55(&max1, DIG2, SEGB);
    MaxLed lm_pzb70(&max1, DIG3, SEGB);
    MaxLed lm_pzb85(&max1, DIG4, SEGB);
    MaxLed lm_pzb1000(&max1, DIG4, SEGE);
    MaxLed lm_pzb500(&max1, DIG3, SEGE);
    MaxLed lm_pzbb40(&max1, DIG2, SEGE);

    //Struktur für Türen-Melder
    zusi_tueren_data tueren;
    MaxLed lm_t1(&max1, DIG2, SEGA);

    //Struktur für SiFa-Melder
    zusi_sifa_data sifa;
    MaxLed lm_sifa(&max1, DIG3, SEGA);

    //Leuchtmelder den jeweiligen Daten zuordnen
    alle_lm[0] = (leuchtmelder_t){ &lm_pzb85, &pzb.lm_za_o, &blink1000 };
    alle_lm[1] = (leuchtmelder_t){ &lm_pzb70, &pzb.lm_za_m, &blink1000 };
    alle_lm[2] = (leuchtmelder_t){ &lm_pzb55, &pzb.lm_za_u, &blink1000 };
    alle_lm[3] = (leuchtmelder_t){ &lm_pzb1000, &pzb.lm_1000hz, &blink1000 };
    alle_lm[4] = (leuchtmelder_t){ &lm_pzb500, &pzb.lm_500hz, &blink1000 };
    alle_lm[5] = (leuchtmelder_t){ &lm_pzbb40, &pzb.lm_befehl, &blink1000 };
    alle_lm[6] = (leuchtmelder_t){ &lm_pzbb40, &pzb.lm_befehl, &blink1000 };
    alle_lm[7] = (leuchtmelder_t){ &lm_t1, &tueren.lm_zs, &blink500 };
    alle_lm[8] = (leuchtmelder_t){ &lm_sifa, &sifa.lm_sifa, &blink500 };

    //Gewünschte Daten anfordern
    zusi->add_needed_data(ZUSI_CAB_DATA, 0x0001, &v);
    zusi->add_needed_data(ZUSI_CAB_DATA, ID_PZBGRUND, &pzb);
    zusi->add_needed_data(ZUSI_CAB_DATA, ID_TUEREN_GRUND, &tueren);
    zusi->add_needed_data(ZUSI_CAB_DATA, ID_SIFA_GRUND, &sifa);

    //Max7219 voreinstellen
    
    max1.set_intensity(10);
    max1.set_decode_mode(0);
    max1.set_scan_limit(7);
    max1.clear();
    lm_stoer.write(true);
    max1.update();

    //Blinktakt starten
    blinken.start(blink_takt);

    //Die Ethernetschnittstelle mit DHCP starten
    eth.connect();
    //Anzeigen der eigenen IP
    SocketAddress a;
    eth.get_ip_address(&a);
    printf("My IP address: %s\n", a.get_ip_address() ? a.get_ip_address() : "None");

    //Den Zusi-Client mit der Ethernet-Schnittstelle starten
    //Der Client läuft in einem eigenen Thread und versucht die Verbindung (wieder)herzustellen
    zusi->start(&eth, "192.168.178.48", 1436);

    while (true) {
        //Statusmeldung vom Client abfragen und Leuchtmelder einschalten wenn nicht Online
        leuchtmelder_update();

        max1.update();
    }
}
