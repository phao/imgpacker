#ifndef X_FLOW_H
#define X_FLOW_H

#define DO_IF(cond, effect) if (cond) { effect; } else (void) 0
#define return_if(cond, val) DO_IF(cond, return (val))
#define goto_if(cond, label) DO_IF(cond, goto label)
#define break_if(cond) DO_IF(cond, break)
#define continue_if(cond) DO_IF(cond, continue)

#endif
