# Run Log

| Profile | delay_ms | Miss % | Overhead | Changes / Notes |
|---------|----------|--------|----------|-----------------|
| A       | 100      | 0.13%  | 1.97x    | Initial test of XOR(1,3) FEC. Miss rate comfortably below 1.0% limit. Result is VALID. |
| B       | 140      | 0.40%  | 1.97x    | Adjusted delay_ms to 140ms to accommodate Profile B's 80ms network delay + 60ms FEC window. Result VALID. |
| B       | 100      | 0.93%  | 1.97x    | Optimized structure to XOR(1,2). The shorter 40ms parity window allows shrinking delay_ms to absolute physical boundary. Result VALID. |
| A       | 60       | 0.60%  | 1.97x    | Final Validation on Profile A. Result VALID. |
| B       | 98       | 0.95%  | 1.97x    | Final Validation on Profile B. Push delay down to 98ms, achieving the experimentally verified global minimum. Result VALID. |
