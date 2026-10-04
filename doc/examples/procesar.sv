// Figura 1 (b): bloque __parallel como dos bloques always_comb independientes.
module procesar (
  input  logic signed [31:0] x,
  output logic signed [31:0] cuadrado,
  output logic signed [31:0] absoluto
);
  always_comb begin
    cuadrado = x * x;
  end

  always_comb begin
    if (x < 0)
      absoluto = -x;
    else
      absoluto = x;
  end
endmodule
