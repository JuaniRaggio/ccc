// Figura 3 (b): bucle for acumulador como FSM (IDLE -> COMPUTE -> DONE).
module sumar (
  input  logic               clk,
  input  logic               rst,
  input  logic               start,
  input  logic signed [31:0] datos [8],
  output logic               done,
  output logic signed [31:0] out
);
  typedef enum logic [1:0] { IDLE, COMPUTE, DONE } estado_t;

  estado_t            estado;
  logic signed [31:0] acum;
  logic        [2:0]  i;

  always_ff @(posedge clk) begin
    if (rst) begin
      estado <= IDLE;
      acum   <= 0;
      i      <= 0;
      done   <= 0;
      out    <= 0;
    end
    else begin
      case (estado)
        IDLE: begin
          done <= 0;
          acum <= 0;
          i    <= 0;
          if (start) estado <= COMPUTE;
        end
        COMPUTE: begin
          acum <= acum + datos[i];
          i    <= i + 1;
          if (i == 7) estado <= DONE;
        end
        DONE: begin
          done   <= 1;
          out    <= acum;
          estado <= IDLE;
        end
        default: estado <= IDLE;
      endcase
    end
  end
endmodule
