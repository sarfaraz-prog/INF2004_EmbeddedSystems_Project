/*
 * test_comms_cmd.c -- PC test of the START/STOP/RESET command handling in comms.c.
 * The platform and lwIP are replaced by stand-ins in stubs/, so it runs on a PC.
 * Each check prints an ID S1-Dnn in the order it runs. Add new checks at the END.
 * Build and run from the repo root:
 *   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -DCOMMS_HOST_TEST -Itesting/subsystem1/stubs \
 *       -Iapp_program/subsystem1 testing/subsystem1/test_comms_cmd.c \
 *       app_program/subsystem1/trans_queue.c app_program/subsystem1/com_state.c \
 *       app_program/subsystem1/status_led.c app_program/subsystem1/telemetry.c -o /tmp/test_comms_cmd
 *   /tmp/test_comms_cmd
 */
#include <stdio.h>
#include <string.h>
#include "fake_port.h"
#include "../../app_program/subsystem1/comms.c"
static int fails=0;
static int checks=0;
#define CHECK(c,m) do{ checks++; if(c) printf("PASS S1-D%02d %s\n",checks,m); else {printf("FAIL S1-D%02d %s\n",checks,m); fails++;} }while(0)
/* one command message = the bytes given (binary now, not text) */
static void msg(const char*topic,const uint8_t*b,uint16_t n){
    inpub_cb(NULL,topic,(u32_t)n);
    indata_cb(NULL,b,n,MQTT_DATA_FLAG_LAST);
}
static void cmd1(uint8_t v){ msg(COMMS_TOPIC_CMD,&v,1); }
int main(void){
    g_cmd_gate = true; g_cmd_open_at = 0;      /* pretend subscribed long ago: this test is about the bytes */
    uint8_t two[2]={1,1}, zero_len=0; (void)zero_len;
    CHECK(comms_get_command()==COMMS_CMD_NONE,"empty at start");
    cmd1(1); CHECK(comms_get_command()==COMMS_CMD_START,"byte 1 = START accepted");
    cmd1(2); CHECK(comms_get_command()==COMMS_CMD_STOP,"byte 2 = STOP accepted");
    cmd1(3); CHECK(comms_get_command()==COMMS_CMD_RESET,"byte 3 = RESET accepted");
    CHECK(comms_get_command()==COMMS_CMD_NONE,"each command returned once");
    uint32_t rej=g_cmds_rejected;
    cmd1(0); cmd1(4); cmd1(255); msg(COMMS_TOPIC_CMD,two,2); msg(COMMS_TOPIC_CMD,two,0);
    CHECK(comms_get_command()==COMMS_CMD_NONE,"bad bytes give no command");
    CHECK(g_cmds_rejected-rej==5,"5 bad messages counted (0, 4, 255, 2 bytes long, empty)");
    rej=g_cmds_rejected;
    { uint8_t v=1; msg("robocar/car1/other",&v,1); }
    CHECK(comms_get_command()==COMMS_CMD_NONE,"other topic ignored");
    CHECK(g_cmds_rejected==rej,"other topic not counted as bad command");
    /* the old text commands must no longer work */
    rej=g_cmds_rejected;
    msg(COMMS_TOPIC_CMD,(const uint8_t*)"START",5); msg(COMMS_TOPIC_CMD,(const uint8_t*)"S",1);
    CHECK(comms_get_command()==COMMS_CMD_NONE && g_cmds_rejected-rej==2,"old text commands rejected");
    /* two pieces for a 2-byte message must be rejected too (a command is 1 byte) */
    rej=g_cmds_rejected;
    inpub_cb(NULL,COMMS_TOPIC_CMD,2);
    indata_cb(NULL,&two[0],1,0);
    indata_cb(NULL,&two[1],1,MQTT_DATA_FLAG_LAST);
    CHECK(comms_get_command()==COMMS_CMD_NONE && g_cmds_rejected-rej>=1,"2-byte message in pieces rejected");
    /* ring holds 3 (size 4), 4th is dropped and counted */
    uint32_t acc=g_cmds_accepted; rej=g_cmds_rejected;
    cmd1(1); cmd1(2); cmd1(3); cmd1(1);
    CHECK(g_cmds_accepted-acc==3 && g_cmds_rejected-rej==1,"ring full: 3 kept, 1 dropped and counted");
    CHECK(comms_get_command()==COMMS_CMD_START && comms_get_command()==COMMS_CMD_STOP && comms_get_command()==COMMS_CMD_RESET,"kept in order");
    CHECK(comms_get_command()==COMMS_CMD_NONE,"then empty");
    /* the comms enum must match the wire codes in data_layout.h */
    CHECK(COMMS_CMD_START==DL_CMD_START && COMMS_CMD_STOP==DL_CMD_STOP && COMMS_CMD_RESET==DL_CMD_RESET,"enum equals wire codes");
    printf("%d checks, %s (%d failed)\n", checks, fails?"FAILED":"ALL PASSED", fails);
    return fails!=0;
}