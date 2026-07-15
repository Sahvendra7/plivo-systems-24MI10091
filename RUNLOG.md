# Run Log

| Profile | delay_ms | Miss % | Overhead | Changes / Notes |
|---------|----------|--------|----------|-----------------|
| A       | 100      | 0.13%  | 1.97x    | Initial test of XOR(1,3) FEC. Miss rate comfortably below 1.0% limit. Result is VALID. |
| B       | 100      | 1.53%  | 1.97x    | Stressed on Profile B with 100ms. High network delay + FEC recovery delay exceeded deadline. Result INVALID. |
| B       | 140      | 0.40%  | 1.97x    | Adjusted delay_ms to 140ms to accommodate Profile B's 80ms network delay + 60ms FEC window. Miss rate dropped well below 1.0%. Result VALID. |
