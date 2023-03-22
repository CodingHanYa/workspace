#pragma once

// ========================================
//   Please forgive my poor english
// ========================================

/**
 * @brief A steady thread pond
 * It is a steady thread pond that has constant count of threads and is suitable for stable task volume and light task.
 * It also support task stealing, batch submission, batch execution, thread load balancing.
 */
#include "hipe/steady_pond.h"

/**
 * @brief A dynamic thread pond.
 * It is a dynamic thread pond that allows expanding the pond with some threads or delete some threads from the pond.
 * To help you know when  and how to motify the pond, it provided interfaces to monitor the speed of task digestion.
 */
#include "hipe/dynamic_pond.h"

/**
 * @brief A balanced thread pond
 * It is a balanced thread pond has outstanding performance and support task stealing, batch submission, batch
 * execution, thread load balancing.
 */
#include "hipe/balanced_pond.h"
