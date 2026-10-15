/*
 * servo.h
 *
 *  Created on: Oct 19, 2023
 *      Author: Kikkiu
 */

#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include "main.h"

#define MAX_ANGLE 180

#define SERVO_MIN_PULSE  500U   // 0.5 ms (0 deg)
#define SERVO_MAX_PULSE  2500U  // 2.5 ms (180 deg)

typedef struct Servo_s
{
    const char* servo_id;           // unique ID string (e.g. "servo1")
    const char* name;               // friendly name for HA (e.g. "Irrigator Servo")
    TIM_HandleTypeDef* htim;        // STM32 Timer handle (e.g. &htim3)
    uint32_t channel;               // timer channel (e.g. TIM_CHANNEL_1)
    uint8_t inverted;               // active low / inverted PWM
    uint8_t min_angle;              // minimum allowed angle (0 .. 180)
    uint8_t max_angle;              // maximum allowed angle (0 .. 180)
    uint8_t current_angle;          // current angle
    uint8_t target_angle;           // target angle
} Servo_t;

static inline void SERVO_Write(TIM_HandleTypeDef *htim, uint32_t channel, uint8_t angle, uint8_t inverted)
{
    if (!htim) return;

    if (angle > MAX_ANGLE) angle = MAX_ANGLE;

	uint32_t period = (uint32_t)angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE) / MAX_ANGLE + SERVO_MIN_PULSE;

    if (inverted)
        period = htim->Instance->ARR - period;

    __HAL_TIM_SET_COMPARE(htim, channel, period);
}

static inline void SERVO_RAWWrite(TIM_HandleTypeDef *htim, uint32_t channel, uint8_t percentage, uint8_t inverted)
{
    if (!htim) return;

    if (percentage > 100)
        percentage = 100;

    uint32_t compare = (uint32_t)htim->Instance->ARR * percentage / 100;
    if (inverted) 
        compare = htim->Instance->ARR - compare;

    __HAL_TIM_SET_COMPARE(htim, channel, compare);
}

static inline void SERVO_WriteWithLimits(TIM_HandleTypeDef *htim, uint32_t channel, uint8_t angle, uint8_t min_angle, uint8_t max_angle, uint8_t inverted)
{
    if (angle < min_angle)
        angle = min_angle;
    if (angle > max_angle)
        angle = max_angle;
    SERVO_Write(htim, channel, angle, inverted);
}

static inline void SERVO_SetAngle(Servo_t* servo, uint8_t angle)
{
    if (!servo) return;

    if (angle < servo->min_angle)
        angle = servo->min_angle;
    if (angle > servo->max_angle)
        angle = servo->max_angle;
    if (angle > MAX_ANGLE)
        angle = MAX_ANGLE;

    servo->current_angle = angle;
    servo->target_angle = angle;

    if (servo->htim)
        SERVO_Write(servo->htim, servo->channel, angle, servo->inverted);
}

static inline void SERVO_SetMinAngle(Servo_t* servo, uint8_t min_angle)
{
    if (!servo) return;

    if (min_angle > MAX_ANGLE)
        min_angle = MAX_ANGLE;
    if (min_angle > servo->max_angle)
        min_angle = servo->max_angle;

    servo->min_angle = min_angle;

    if (servo->current_angle < min_angle)
        SERVO_SetAngle(servo, min_angle);
}

static inline void SERVO_SetMaxAngle(Servo_t* servo, uint8_t max_angle)
{
    if (!servo) return;

    if (max_angle > MAX_ANGLE)
        max_angle = MAX_ANGLE;
    if (max_angle < servo->min_angle)
        max_angle = servo->min_angle;

    servo->max_angle = max_angle;

    if (servo->current_angle > max_angle)
        SERVO_SetAngle(servo, max_angle);
}

static inline void SERVO_Init(Servo_t* servo, const char* servo_id, const char* name,
                              TIM_HandleTypeDef* htim, uint32_t channel,
                              uint8_t min_angle, uint8_t max_angle, uint8_t inverted)
{
    if (!servo) return;

    servo->servo_id = servo_id;
    servo->name = name;
    servo->htim = htim;
    servo->channel = channel;
    servo->inverted = inverted;

    if (min_angle > MAX_ANGLE) min_angle = MAX_ANGLE;
    if (max_angle > MAX_ANGLE) max_angle = MAX_ANGLE;
    if (min_angle > max_angle) min_angle = max_angle;

    servo->min_angle = min_angle;
    servo->max_angle = max_angle;
    servo->current_angle = min_angle;
    servo->target_angle = min_angle;

    if (htim) 
    {
        HAL_TIM_PWM_Start(htim, channel);
        SERVO_Write(htim, channel, servo->current_angle, servo->inverted);
    }
}

static inline void SERVO_Start(Servo_t* servo)
{
    if (servo && servo->htim)
    {
        HAL_TIM_PWM_Start(servo->htim, servo->channel);
        SERVO_Write(servo->htim, servo->channel, servo->current_angle, servo->inverted);
    }
}

static inline void SERVO_Stop(Servo_t* servo)
{
    if (servo && servo->htim)
        HAL_TIM_PWM_Stop(servo->htim, servo->channel);
}

#endif /* INC_SERVO_H_ */
