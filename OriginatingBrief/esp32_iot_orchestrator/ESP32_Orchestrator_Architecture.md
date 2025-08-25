
# ESP32 IoT Orchestrator — Software Architecture (Authoritative Spec)
... (truncated in this cell for brevity in code) ...

---

## 13. New Execution Interfaces (Augmented)

**New variables and methods added based on design evolution:**

- **`NextCheckMs`**:  
  - `uint32_t NextCheckMs` added as a reference variable maintained by the orchestrator.  
  - Default is 0. Each call to `execute()` can update `NextCheckMs` to inform the orchestrator when the component wishes to run next.

- **`ExecuteNow(...)`**:  
  - Added to `BaselineComponent` as:  
    ```cpp
    virtual bool ExecuteNow(const String& inputJson, String& outputJson, int* resultCode, String* resultDescription) = 0;
    ```
  - Allows a caller (e.g., web server) to invoke a component immediately with supplied JSON parameters and get back serialized output and status.

- **`ExecuteOptions()`**:  
  - `BaselineComponent` exposes:  
    ```cpp
    virtual String ExecuteOptions() const = 0;
    ```
  - Returns a serialized JSON string describing the execution options or supported commands for this component.  
  - Can be overridden by child classes to describe their capabilities.

These additions allow the orchestrator and front-end stacks to query available commands and trigger one-off actions, complementing the scheduled `execute()` cycle.
