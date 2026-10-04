// Figura 4 (b): potencia y LUT se resuelven en tiempo de compilación
// y no generan hardware; solo queda el localparam con los valores.
module buscar (
  input  logic signed [31:0] idx,
  output logic signed [31:0] out
);
  localparam int N = 4;
  // Vector aplanado en lugar del arreglo {1, 2, 4, 8} del informe: ambos
  // son SystemVerilog válido, pero Icarus Verilog 12 no soporta arreglos
  // (unpacked ni packed multidimensionales) como localparam.
  localparam logic [N*32-1:0] LUT = {32'd8, 32'd4, 32'd2, 32'd1};

  always_comb begin
    out = LUT[idx[1:0]*32 +: 32];
  end
endmodule
