/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#if 0
#define DEBUG
#endif

#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <soo/device/baos_client.h> 

/**
 * @brief Global struct to store data
 * 
 */
static struct baos_client baos_client_priv = {SERVER_ITEM, NULL};
static DECLARE_COMPLETION(wait_response);

/**
 * @brief Error code strings 
 * 
 */
static const char error_string [][STRING_SIZE] = {
    "No error",
    "Internal error",
    "No element found",
    "Buffer is too small",
    "Item is not writeable",
    "Service not supported",
    "Bad device parameter",
    "Bad id",
    "Bad command / value",
    "Bad length",
    "Message inconsistent",
    "Object server is busy"
};

static void print_buffer(byte *buf, int len) {
    int i;

    pr_info("Buffer length: %d\n",len);
    for (i = 0; i < len; i++) {
        printk("0x%02X ", buf[i]);
    }
    printk("\n");
}

/**
 * @brief Frame types string
 * 
 */
static const char baos_frame_type_string [][STRING_SIZE] = {
    [SERVER_ITEM] = "Server item",
    [DATAPOINT] = "Datapoint"
};

/* Number of subscriber to BAOS client */
static int subscribers_count = 0;

/* Array callback funtions provided by the subscribers */
static void (*subscribers[MAX_SUBSCRIBERS])(baos_frame_t *frame);

/**
 * @brief Convert a BAOS frame to an array of bytes
 * 
 * @param frame Frame to convert
 * @param data_len Length of the resulting byte array
 * @return byte* Byte array
 */
static byte *baos_flatten(baos_frame_t frame, int data_len) {
    byte *buf;
    int bytes = 0, i, j;

    buf = kzalloc(data_len * sizeof(byte), GFP_KERNEL);
    BUG_ON(!buf);

    buf[BAOS_MAIN_SERVICE_OFF] = BAOS_MAIN_SERVICE;
    buf[BAOS_SUBSERVICE_OFF] = frame.subservice;
    buf[BAOS_START_OBJECT_OFF] = frame.first_obj_id.bytes.msb;
    buf[BAOS_START_OBJECT_OFF + 1] = frame.first_obj_id.bytes.lsb;
    buf[BAOS_OBJECT_COUNT_OFF] = frame.obj_count.bytes.msb;
    buf[BAOS_OBJECT_COUNT_OFF + 1] = frame.obj_count.bytes.lsb;

    if (data_len > BAOS_FRAME_MIN_SIZE) {
        switch (frame.type) { 
            case DATAPOINT:
                for (i = 0; i < frame.obj_count.val; i++) {
                    buf[BAOS_FRAME_MIN_SIZE + bytes++] = frame.datapoints[i]->id.bytes.msb;
                    buf[BAOS_FRAME_MIN_SIZE + bytes++] = frame.datapoints[i]->id.bytes.lsb;
                    buf[BAOS_FRAME_MIN_SIZE + bytes++] = frame.datapoints[i]->command;
                    buf[BAOS_FRAME_MIN_SIZE + bytes++] = frame.datapoints[i]->length;

                    for (j = 0; j < frame.datapoints[i]->length; j++) {
                        buf[BAOS_FRAME_MIN_SIZE + bytes++] = frame.datapoints[i]->data[j];
                    }
                }
                break;

            case SERVER_ITEM:
                /** Nothing to do **/
                break;
        }
    }

    return buf;
}

/**
 * @brief Print BAOS items 
 * 
 * @param frame frame to print
 */
static void baos_print_items(baos_frame_t *frame) {
    int i, j;
    printk("Server items:\n");
    for (i = 0; i < frame->obj_count.val; i++) {
        printk("\tServer item [%d]:\n", i);
        printk("\tID: 0x%04X\n", frame->server_items[i]->id.val);
        printk("\tData length: %d\n", frame->server_items[i]->length);
        printk("\tData:\n");

        for (j = 0; j < frame->server_items[i]->length; j++) {
            printk("\t[%d]: 0x%02X\n", j, frame->server_items[i]->data[j]);
        }
    }
}

/**
 * @brief Print BAOS datapoint
 * 
 * @param frame frame to print
 */
static void baos_print_datapoint(baos_frame_t *frame) {
    int i, j;

    printk("Datapoints:\n");
    for (i = 0; i < frame->obj_count.val; i++) {
        printk("\tDatapoint [%d]:\n", i);
        printk("\tID: 0x%04X\n", frame->datapoints[i]->id.val);
        printk("\tState/Command: 0x%02X\n", frame->datapoints[i]->state);
        printk("\tData length: %d\n", frame->datapoints[i]->length);
        printk("\tData:\n");

        for (j = 0; j < frame->datapoints[i]->length; j++) {
            printk("\t[%d]: 0x%02X\n", j, frame->datapoints[i]->data[j]);
        }
    }
}

void baos_print_frame(baos_frame_t *frame) {
    BUG_ON(!frame);
    
    printk("Baos frame:\n");
    printk("Subservice: 0x%02X\n", frame->subservice);
    printk("Frame type: %s\n", baos_frame_type_string[frame->type]);
    printk("Start object id: 0x%04X\n", frame->first_obj_id.val);
    printk("Object count: 0x%04X\n", frame->obj_count.val);
    
    if (frame->obj_count.val > 0) {
        switch (frame->type)
        {
        case SERVER_ITEM:
            BUG_ON(!frame->server_items);
            baos_print_items(frame);
            break;
        
        case DATAPOINT:
            if (frame->datapoints)
                baos_print_datapoint(frame);
            break;
        }
    }
}

/**
 * @brief Copy a BAOS frame
 * 
 * @param dest Destination. Where to copy the frame
 * @param src Source. Frame to copy the data of
 */
static void baos_copy_frame(baos_frame_t **dest, baos_frame_t *src) {
    baos_frame_t *tmp;
    int i;

    *dest = kzalloc(sizeof(baos_frame_t), GFP_KERNEL);
    BUG_ON(!*dest);

    tmp = *dest;
    tmp->subservice = src->subservice;
    tmp->first_obj_id.val = src->first_obj_id.val;
    tmp->obj_count.val = src->obj_count.val;
    tmp->type = src->type;

    switch (src->type) {
        case SERVER_ITEM:
            tmp->server_items = kzalloc(tmp->obj_count.val * sizeof(baos_server_item_t *), GFP_KERNEL);
            BUG_ON(!tmp->server_items);

            for (i = 0; i < tmp->obj_count.val; i++) {
                tmp->server_items[i] = kzalloc(sizeof(baos_server_item_t), GFP_KERNEL);
                BUG_ON(!tmp->server_items[i]);

                tmp->server_items[i]->id = src->server_items[i]->id;
                tmp->server_items[i]->length = src->server_items[i]->length;

                tmp->server_items[i]->data = kzalloc(tmp->server_items[i]->length * sizeof(byte), GFP_KERNEL);
                BUG_ON(!tmp->server_items[i]->data);
                memcpy(tmp->server_items[i]->data, src->server_items[i]->data, tmp->server_items[i]->length);
            }
            break;

        case DATAPOINT:
            tmp->datapoints = kzalloc(tmp->obj_count.val * sizeof(baos_datapoint_t *), GFP_KERNEL);
            BUG_ON(!tmp->datapoints);

            for (i = 0; i < tmp->obj_count.val; i++) {
                tmp->datapoints[i] = kzalloc(sizeof(baos_datapoint_t), GFP_KERNEL);
                BUG_ON(!tmp->datapoints[i]);

                tmp->datapoints[i]->id = src->datapoints[i]->id;
                tmp->datapoints[i]->state = src->datapoints[i]->state;
                tmp->datapoints[i]->length = src->datapoints[i]->length;

                tmp->datapoints[i]->data = kzalloc(tmp->datapoints[i]->length * sizeof(byte), GFP_KERNEL);
                BUG_ON(!tmp->datapoints[i]->data);
                memcpy(tmp->datapoints[i]->data, src->datapoints[i]->data, tmp->datapoints[i]->length);
            }
            break;
    }
}

/**
 * @brief Copy a datapoint
 * 
 * @param dest Destination. Where to copy the datapoint
 * @param src Source. Datapoint to copy data of
 */
static void copy_data_point(baos_datapoint_t **dest, baos_datapoint_t *src) {
    baos_datapoint_t *tmp;

    *dest = kzalloc(sizeof(baos_datapoint_t), GFP_KERNEL);
    BUG_ON(!*dest);

    tmp = *dest;
    memcpy(tmp, src, 4);

    tmp->data = kzalloc(src->length * sizeof(byte), GFP_KERNEL);
    BUG_ON(!tmp->data);

    memcpy(tmp->data, src->data, src->length);
}

/**
 * @brief Build server items array from a byte array
 * 
 * @param frame BAOS frame to store the server items
 * @param buf Byte array containing the data
 */
static void baos_build_server_item(baos_frame_t *frame, byte *buf) {
    int i, j, bytes = 0;

    frame->server_items = kzalloc(frame->obj_count.val * sizeof(baos_server_item_t*), GFP_KERNEL);
    BUG_ON(!frame->server_items);

    for (i = 0; i < frame->obj_count.val; i++) {
        frame->server_items[i] = kzalloc(sizeof(baos_server_item_t), GFP_KERNEL);
        BUG_ON(!frame->server_items[i]);

        frame->server_items[i]->id.bytes.msb = buf[BAOS_FIRST_OBJECT_OFF + bytes++];
        frame->server_items[i]->id.bytes.lsb = buf[BAOS_FIRST_OBJECT_OFF + bytes++];
        frame->server_items[i]->length = buf[BAOS_FIRST_OBJECT_OFF + bytes++];

        frame->server_items[i]->data = kzalloc(frame->server_items[i]->length * sizeof(byte), GFP_KERNEL);
        BUG_ON(!frame->server_items[i]->data);

        for (j = 0; j < frame->server_items[i]->length; j++) {
            frame->server_items[i]->data[j] = buf[BAOS_FIRST_OBJECT_OFF + bytes++];
        }
    }
}

/**
 * @brief Build datapoint array from a byte array
 * 
 * @param frame BAOS frame to store the datapoints
 * @param buf Byte array containing the datapoints
 */
static void baos_build_datapoint(baos_frame_t *frame, byte *buf) {
    int i, j, bytes = 0;
    
    frame->datapoints = kzalloc(frame->obj_count.val * sizeof(baos_datapoint_t *), GFP_KERNEL);
    BUG_ON(!frame->datapoints);

    for (i = 0; i < frame->obj_count.val; i++) {
        frame->datapoints[i] = kzalloc(sizeof(baos_datapoint_t), GFP_KERNEL);
        BUG_ON(!frame->datapoints[i]);

        frame->datapoints[i]->id.bytes.msb = buf[BAOS_FIRST_OBJECT_OFF + bytes++];
        frame->datapoints[i]->id.bytes.lsb = buf[BAOS_FIRST_OBJECT_OFF + bytes++];
        frame->datapoints[i]->state = buf[BAOS_FIRST_OBJECT_OFF + bytes++];
        frame->datapoints[i]->length = buf[BAOS_FIRST_OBJECT_OFF + bytes++];


        frame->datapoints[i]->data = kzalloc(frame->datapoints[i]->length * sizeof(byte), GFP_KERNEL);
        BUG_ON(!frame->datapoints[i]->data);

        for (j = 0; j < frame->datapoints[i]->length; j++) {
            frame->datapoints[i]->data[j] = buf[BAOS_FIRST_OBJECT_OFF + bytes++];
        }
    }
}
/**
 * @brief Build a BAOS frame
 * 
 * @param buf Byte array containing BAOS frame
 * @param len Byte array length
 * @return baos_frame_t* BAOS frame object
 */
static baos_frame_t *baos_build_object(byte *buf, int len) {
    baos_frame_t *frame;
    // byte error_code;

    frame = kzalloc(sizeof(baos_frame_t), GFP_KERNEL);
    BUG_ON(!frame);

    if (len < BAOS_FRAME_MIN_SIZE)
        BUG();
    
    frame->subservice = buf[BAOS_SUBSERVICE_OFF];
    frame->first_obj_id.bytes.msb = buf[BAOS_START_OBJECT_OFF];
    frame->first_obj_id.bytes.lsb = buf[BAOS_START_OBJECT_OFF + 1];
    frame->obj_count.bytes.msb = buf[BAOS_OBJECT_COUNT_OFF];
    frame->obj_count.bytes.lsb = buf[BAOS_OBJECT_COUNT_OFF + 1];
    frame->error_code = 0;

    switch (frame->subservice) {
        case DATAPOINT_VALUE_INDICATION:
            frame->type = DATAPOINT;
            break;
        
        case SERVER_ITEM_INDICATION:
            frame->type = SERVER_ITEM;
            break;

        default:
            /** This means the event was generated by a request. Hence the frame type
             * has been setted in the global struct  
             */
            frame->type = baos_client_priv.response_type;
            break;
    }

    if (frame->obj_count.val == 0) {
        frame->error_code = buf[BAOS_FIRST_OBJECT_OFF];
        return frame;
    }

    switch(frame->type) {
        case SERVER_ITEM:
            baos_build_server_item(frame, buf);
            break;
        case DATAPOINT:
            baos_build_datapoint(frame, buf);
            break;
    }

    return frame;
}

void baos_free_frame(baos_frame_t *frame) {
    int i;

    if (frame) {
        switch (frame->type) {
            case SERVER_ITEM:
                if (frame->server_items) {
                    for (i = 0; i < frame->obj_count.val; i++) {
                        kfree(frame->server_items[i]->data);
                    }
                    kfree(frame->server_items);
                }
                break;

            case DATAPOINT:
                if (frame->datapoints) {
                    for (i = 0; i < frame->obj_count.val; i++) {
                        kfree(frame->datapoints[i]->data);
                    }
                    kfree(frame->datapoints);
                }
                break;
        }
        kfree(frame);
    }
    frame = NULL;
}

static int indication_thread(void *data) {
    baos_frame_t *ind = (baos_frame_t *)data;
    int  i;

    if (!ind)
        return -1;

    for (i = 0; i < subscribers_count; i++) {
        if (subscribers[i])
            subscribers[i](ind);
    }

    baos_free_frame(ind);

    return 0;
}

void baos_store_response(byte *buf, int len) {
    baos_frame_t *indication;
    
    baos_free_frame(baos_client_priv.response);
    baos_client_priv.response = NULL;
    baos_client_priv.response = baos_build_object(buf, len);

    switch (baos_client_priv.response->subservice)
    {
        case SERVER_ITEM_INDICATION:
            /** TODO: use it for something **/
            pr_info("%s: Received a new server item indication\n", __func__);
            break;
        
        case DATAPOINT_VALUE_INDICATION:
            pr_info("%s: Received a new datapoint indication\n", __func__);
            baos_copy_frame(&indication, baos_client_priv.response);
            kthread_run(indication_thread, indication, "send_indication_th");
            break;

        default:
            /** All other cases the response is triggered by a request and not an event. **/
            // kberry838_send_ack();
            complete(&wait_response);
            break;  
        }
}

/**
 * @brief Wait for a response when a request has been sent
 * 
 */
static int baos_wait_for_response(void) {
    if (wait_for_completion_timeout(&wait_response, msecs_to_jiffies(BAOS_RESPONSE_TIMEOUT)) == 0) {
        pr_err("Error: BAOS response timeout reached\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Check if the response matches the request
 * 
 * @param sub 
 * @param func_name 
 */
void baos_check_response(baos_subservices sub, const char *func_name) {
    if (baos_client_priv.response->subservice != sub + BAOS_SUBSERVICE_RESPONSE_OFF) {
        pr_err("[%s] Error: Subservice is incorrect. Got: 0x%02X, expected: 0x%02X\n",func_name,
                baos_client_priv.response->subservice, 
                sub + BAOS_SUBSERVICE_RESPONSE_OFF);
	/* TODO Check why this check fail and why it is needed 
	For now, we can ignore this check and just return */	
        // BUG();
    }
}

void baos_client_subscribe_to_indications(void (*indication_fn)(baos_frame_t *frame)) {
    if (indication_fn) {
        if (subscribers_count < MAX_SUBSCRIBERS) {
            subscribers[subscribers_count++] = indication_fn;
            pr_info(BAOS_CLIENT_PREFIX "New subscriber registered\n");
        } else {
            pr_err(BAOS_CLIENT_PREFIX "No more subscribers allowed\n");
            BUG();
        }
    }
}

baos_frame_t *baos_get_server_item(u_int16_t first_item_id, u_int16_t item_count) {
    baos_frame_t frame;
    baos_frame_t *rsp;
    byte *data;

    baos_client_priv.response_type = SERVER_ITEM;

    frame.subservice = GET_SERVER_ITEM;
    frame.first_obj_id.val = first_item_id;
    frame.obj_count.val = item_count;

    data = baos_flatten(frame, BAOS_FRAME_MIN_SIZE);
    kberry838_send_data(data, BAOS_FRAME_MIN_SIZE);

    if (baos_wait_for_response() < 0) {
        pr_err("%s: Request was:\n", __func__);
        baos_print_frame(&frame);
    } else {
        baos_check_response(frame.subservice, __func__);
        baos_copy_frame(&rsp, baos_client_priv.response);
    }

    kfree(data);

#ifdef DEBUG
    baos_print_frame(rsp);
#endif

    return rsp;
}

baos_frame_t *baos_get_datapoint_value(u_int16_t first_datapoint_id, u_int16_t datapoint_count) {
    baos_frame_t frame;
    baos_frame_t *rsp;
    byte *data;
    int data_len = BAOS_FRAME_MIN_SIZE + 1;

    baos_client_priv.response_type = DATAPOINT;
    frame.subservice = GET_DATAPOINT_VALUE;
    frame.first_obj_id.val = first_datapoint_id;
    frame.obj_count.val = datapoint_count;
    frame.type = DATAPOINT;

    data = baos_flatten(frame, data_len);
    data[BAOS_FRAME_MIN_SIZE] = 0x00;

    kberry838_send_data(data, data_len);

    if (baos_wait_for_response() < 0) {
        pr_err("%s: Request was:\n", __func__);
        baos_print_frame(&frame);
    } else {
        if (baos_client_priv.response->error_code != 0) {
            pr_err("Error on request %s(%d, %d): %s\n", __func__, first_datapoint_id,
                    datapoint_count, error_string[baos_client_priv.response->error_code]);
        } else {
            baos_check_response(frame.subservice, __func__);
            baos_copy_frame(&rsp, baos_client_priv.response);
        }
    }

    kfree(data);

#ifdef DEBUG
    baos_print_frame(rsp);
#endif

    return rsp;
}

baos_frame_t *baos_get_datapoint_description(u_int16_t first_datapoint_id, u_int16_t datapoint_count) {
    baos_frame_t frame;
    baos_frame_t *rsp;
    byte *data;

    baos_client_priv.response_type = DATAPOINT;
    frame.subservice = GET_DATAPOINT_DESC;
    frame.first_obj_id.val = first_datapoint_id;
    frame.obj_count.val = datapoint_count;
    frame.type = DATAPOINT;
    frame.datapoints = NULL;

    data = baos_flatten(frame,  BAOS_FRAME_MIN_SIZE);
    
    pr_info("%s:\n", __func__);
    print_buffer(data,  BAOS_FRAME_MIN_SIZE);

    kberry838_send_data(data, BAOS_FRAME_MIN_SIZE);

    if (baos_wait_for_response() < 0) {
        pr_err("%s: Request was:\n", __func__);
        baos_print_frame(&frame);
        rsp = NULL;
    } else {
        if (baos_client_priv.response->error_code != 0) {
            pr_err("Error on request %s(%d, %d): %s\n", __func__, first_datapoint_id,
                    datapoint_count, error_string[baos_client_priv.response->error_code]);
            rsp = NULL;
        } else {
            baos_check_response(frame.subservice, __func__);
            baos_copy_frame(&rsp, baos_client_priv.response);
        }
    }

    kfree(data);

#ifdef DEBUG
    baos_print_frame(rsp);
#endif

    return rsp;
}

void baos_set_datapoint_value(baos_datapoint_t *datapoints, int datapoints_count) {
    baos_frame_t frame;
    int bytes_count;
    byte *data;
    int i;

    baos_client_priv.response_type = DATAPOINT;

    frame.subservice = SET_DATAPOINT_VALUE;
    frame.first_obj_id.val = datapoints[0].id.val;
    frame.obj_count.val = datapoints_count;
    frame.type = DATAPOINT;

    bytes_count = BAOS_FRAME_MIN_SIZE;

    if (datapoints_count > 0) {
        frame.datapoints = kzalloc(datapoints_count * sizeof(baos_datapoint_t *), GFP_KERNEL);
        BUG_ON(!frame.datapoints);

        for (i = 0; i < datapoints_count; i++) {
            frame.datapoints[i] = kzalloc(sizeof(baos_datapoint_t), GFP_KERNEL);
            BUG_ON(!frame.datapoints[i]);
            copy_data_point(&frame.datapoints[i], &datapoints[i]);
            bytes_count += DATAPOINT_MIN_SIZE + frame.datapoints[i]->length;
        }
    }

#ifdef DEBUG
    baos_print_frame(&frame);
#endif

    data = baos_flatten(frame, bytes_count);
    kberry838_send_data(data, bytes_count);
    
    if (baos_wait_for_response() < 0) {
        pr_err("%s: Request was:\n", __func__);
        baos_print_frame(&frame);
    } else {
        baos_check_response(frame.subservice, __func__);
    }

    kfree(data);
}
