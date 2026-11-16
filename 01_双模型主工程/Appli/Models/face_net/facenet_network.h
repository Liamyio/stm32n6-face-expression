/**
  ******************************************************************************
  * @file    facenet.h
  * @author  STEdgeAI
  * @date    2026-07-28 19:16:24
  * @brief   Minimal description of the generated c-implemention of the network
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef LL_ATON_FACENET_H
#define LL_ATON_FACENET_H

/******************************************************************************/
#define LL_ATON_FACENET_C_MODEL_NAME        "facenet"
#define LL_ATON_FACENET_ORIGIN_MODEL_NAME   "rfb320_int8_qdq"

/************************** USER ALLOCATED IOs ********************************/
// No user allocated inputs
// No user allocated outputs

/************************** INPUTS ********************************************/
#define LL_ATON_FACENET_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_5_out_0
#define LL_ATON_FACENET_IN_1_ALIGNMENT   (32)
#define LL_ATON_FACENET_IN_1_SIZE_BYTES  (230400)

/************************** OUTPUTS *******************************************/
#define LL_ATON_FACENET_OUT_NUM        (2)    // Total number of output buffers
// Output buffer 1 -- Quantize_205_out_0
#define LL_ATON_FACENET_OUT_1_ALIGNMENT   (32)
#define LL_ATON_FACENET_OUT_1_SIZE_BYTES  (8840)
// Output buffer 2 -- Quantize_240_out_0
#define LL_ATON_FACENET_OUT_2_ALIGNMENT   (32)
#define LL_ATON_FACENET_OUT_2_SIZE_BYTES  (17680)

#endif /* LL_ATON_FACENET_H */
