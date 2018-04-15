/*
 * OTA dual app
 * use local.mk to turn it into the OTA otamain.bin app or the otaboot.bin app
 */

#include <stdlib.h>  //for printf and free
#include <stdio.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <wifi_config.h>
#include <string.h>  //for stdcmp
#include <sysparam.h>

#include <ota.h>

void ota_task(void *arg) {
    int holdoff_time=1; //32bit, in seconds
    char* user_repo=NULL;
    char* user_version=NULL;
    char* user_file=NULL;
    char*  new_version=NULL;
    char*  ota_version=NULL;
    signature_t signature;
    extern int active_cert_sector;
    extern int backup_cert_sector;
    int file_size; //32bit
    int have_private_key=0;
    int keyid,foundkey=0;
    char keyname[KEYNAMELEN];
    
    if (ota_boot()) printf("OTABOOT "); else printf("OTAMAIN ");
    printf("VERSION: %s, compiled %s %s\n",OTAVERSION, __DATE__, __TIME__);

    ota_init();
    
    if (!active_cert_sector) {
        active_cert_sector=HIGHERCERTSECTOR;
        backup_cert_sector=LOWERCERTSECTOR;
        ota_version=ota_get_version(OTAREPO);
        ota_get_file(OTAREPO,ota_version,CERTFILE,active_cert_sector);
        ota_finalize_file(active_cert_sector);
    }
    printf("active_cert_sector: 0x%05x\n",active_cert_sector);
    file_size=ota_get_pubkey(active_cert_sector);
    
    if (!ota_get_privkey()) { //have private key
        have_private_key=1;
        printf("have private key\n");
        if (ota_verify_pubkey()) ota_sign(active_cert_sector,file_size, &signature, "pub-1.key");//use this (old) privkey to sign the (new) pubkey
    }

    if (ota_boot()) ota_write_status("0.0.0");  //we will have to get user code from scratch if running ota_boot
    if ( !ota_load_user_app(&user_repo, &user_version, &user_file)) { //repo/version/file must be configured
        //new_version=ota_get_version(user_repo); //consider that if here version is equal, we end it already
        //if (!ota_compare(new_version,user_version)) { //allows a denial of update so not doing it for now
        for (;;) { //escape from this loop by continue (try again) or break (boots into slot 0)
            printf("--- looping\n");
            //printf("%d\n",sdk_system_get_time()/1000);
            //need for a protection against an electricity outage recovery storm
            vTaskDelay(holdoff_time*(1000/portTICK_PERIOD_MS));
            holdoff_time*=HOLDOFF_MULTIPLIER; holdoff_time=(holdoff_time<HOLDOFF_MAX) ? holdoff_time : HOLDOFF_MAX;
            
            //do we still have a valid internet connexion? dns resolve github... should not be private IP
            
            ota_set_verify(0); //should work even without certificates
            if ( ota_version) free( ota_version);
            if ( new_version) free( new_version);
            ota_version=ota_get_version(OTAREPO);
            if (ota_get_hash(OTAREPO, ota_version, CERTFILE, &signature)) { //no certs.sector.sig exists yet on server
                if (have_private_key) {
                    ota_sign(active_cert_sector,SECTORSIZE, &signature, CERTFILE); //reports to console
                    vTaskDelete(NULL); //upload the signature out of band to github and start again
                } else {
                    continue; //loop and try again later
                }
            }
            if (ota_verify_hash(active_cert_sector,&signature)) { //seems we need to download certificates
                ota_get_file(OTAREPO,ota_version,CERTFILE,backup_cert_sector);
                if (ota_verify_hash(backup_cert_sector,&signature)) { //hash and file do not match
                    break; //leads to boot=0
                }
                if (ota_verify_signature(&signature)) { //maybe an update on the public key
                    keyid=1;
                    while (sprintf(keyname,KEYNAME,keyid) , ota_get_hash(OTAREPO, ota_version, keyname, &signature)) {
                        if (!ota_verify_signature(&signature)) {foundkey=1; break;}
                        keyid++;
                    }
                    if (!foundkey) break; //leads to boot=0
                    //we found the head of the chain of pubkeys
                    while (--keyid) {
                        if (ota_get_newkey(OTAREPO,ota_version,keyname,&signature)) {foundkey=0; break;}//contains a check of hash inside
                        sprintf(keyname,KEYNAME,keyid);
                        if (ota_get_hash(OTAREPO,ota_version,keyname,&signature)) {foundkey=0; break;}
                        if (ota_verify_signature(&signature)) {foundkey=0; break;}
                    }
                    if (!foundkey) break; //leads to boot=0
                    //now lets check if the backup_cert_sector contains the true pubkey
                    if (ota_verify_hash(backup_cert_sector,&signature)) break; //leads to boot=0
                }
                ota_swap_cert_sector();
                ota_get_pubkey(active_cert_sector);
            } //certificates are good now
            ota_set_verify(1); //reject faked server
            if (ota_get_hash(OTAREPO, ota_version, CERTFILE, &signature)) { //testdownload, if server is fake will trigger
                //report by syslog?  //trouble, so abort
                break; //leads to boot=0
            }
            if (ota_boot()) { //running the ota-boot software now
                //take care our boot code gets a signature by loading it in boot1sector just for this purpose
                if (ota_get_hash(OTAREPO, ota_version, BOOTFILE, &signature)) { //no signature yet
                    if (have_private_key) {
                        file_size=ota_get_file(OTAREPO,ota_version,BOOTFILE,BOOT1SECTOR);
                        if (file_size<=0) continue; //try again later
                        ota_finalize_file(BOOT1SECTOR);
                        ota_sign(BOOT1SECTOR,file_size, &signature, BOOTFILE); //reports to console
                        vTaskDelete(NULL); //upload the signature out of band to github and start again
                    }
                }
                //now get the latest ota main software in boot sector 1
                if (ota_get_hash(OTAREPO, ota_version, MAINFILE, &signature)) { //no signature yet
                    if (have_private_key) {
                        file_size=ota_get_file(OTAREPO,ota_version,MAINFILE,BOOT1SECTOR);
                        if (file_size<=0) continue; //try again later
                        ota_finalize_file(BOOT1SECTOR);
                        ota_sign(BOOT1SECTOR,file_size, &signature, MAINFILE); //reports to console
                        vTaskDelete(NULL); //upload the signature out of band to github and start again
                    } else {
                        continue; //loop and try again later
                    }
                } else { //we have a signature, maybe also the main file?
                    if (ota_verify_hash(BOOT1SECTOR,&signature)) { //not yet downloaded
                        file_size=ota_get_file(OTAREPO,ota_version,MAINFILE,BOOT1SECTOR);
                        if (file_size<=0) continue; //try again later
                        if (ota_verify_hash(BOOT1SECTOR,&signature)) continue; //download failed
                        ota_finalize_file(BOOT1SECTOR);
                    }
                } //now file is here for sure and matches hash
                if (ota_verify_signature(&signature)) vTaskDelete(NULL); //this should never happen
                ota_temp_boot(); //launches the ota software in bootsector 1
            } else {  //running ota-main software now
                printf("--- running ota-main software\n");
                //if there is a newer version of ota-main...
                if (ota_compare(ota_version,OTAVERSION)>0) { //set OTAVERSION when running make and match with github
                    ota_get_hash(OTAREPO, ota_version, BOOTFILE, &signature);
                    file_size=ota_get_file(OTAREPO,ota_version,BOOTFILE,BOOT0SECTOR);
                    if (file_size<=0) continue; //something went wrong, but now boot0 is broken so start over
                    if (ota_verify_signature(&signature)) continue; //this should never happen
                    if (ota_verify_hash(BOOT0SECTOR,&signature)) continue; //download failed
                    ota_finalize_file(BOOT0SECTOR);
                    break; //leads to boot=0 and starts self-updating/otaboot-app
                } //ota code is up to date
                new_version=ota_get_version(user_repo);
                if (ota_compare(new_version,user_version)>0) { //can only upgrade
                    ota_get_hash(user_repo, new_version, user_file, &signature);
                    file_size=ota_get_file(user_repo,new_version,user_file,BOOT0SECTOR);
                    if (file_size<=0 || ota_verify_hash(BOOT0SECTOR,&signature)) continue; //something went wrong, but now boot0 is broken so start over
                    ota_finalize_file(BOOT0SECTOR);
                } //nothing to update
                ota_write_status(new_version); //we have been successful, hurray!
                break; //leads to boot=0 and starts updated user app
            }
        }
    }
    ota_reboot(); //boot0, either the user program or the otaboot app
    vTaskDelete(NULL); //just for completeness sake, would never get till here
}

void on_wifi_ready() {
    xTaskCreate(ota_task,"ota",4096,NULL,1,NULL);
    printf("wifiready-done\n");
}

void user_init(void) {
//    uart_set_baud(0, 74880);
    uart_set_baud(0, 115200);

    wifi_config_init("OTA", NULL, on_wifi_ready); //expanded it with setting repo-details
    printf("user-init-done\n");
}
