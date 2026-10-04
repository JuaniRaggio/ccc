// Figura 2 (b): condicional como multiplexor.
module clamp (
  input  logic signed [31:0] x,
  input  logic signed [31:0] tope,
  output logic signed [31:0] out
);
  always_comb begin
    if (x > tope)
      out = tope;
    else
      out = x;
  end
endmodule
