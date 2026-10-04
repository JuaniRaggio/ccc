module sumar_tb;
  logic               clk = 0, rst = 1, start = 0, done;
  logic signed [31:0] datos [8];
  logic signed [31:0] out;
  int                 ciclos = 0;

  sumar dut (.clk(clk), .rst(rst), .start(start), .datos(datos), .done(done), .out(out));

  always #5 clk = ~clk;

  initial begin
    foreach (datos[k]) datos[k] = k + 1;   // 1 + 2 + ... + 8 = 36
    // Las entradas se manejan y las salidas se leen en el flanco de bajada,
    // para no competir con el flanco de subida en el que avanza la FSM.
    @(negedge clk); rst = 0;
    @(negedge clk); start = 1;
    @(negedge clk); start = 0;
    while (!done) begin
      @(negedge clk);
      ciclos++;
      if (ciclos > 20) $fatal(1, "timeout");
    end
    if (out !== 36) $fatal(1, "sumar = %0d, expected 36", out);
    $display("sumar(1..8) = %0d (%0d ciclos)", out, ciclos);
    $display("PASS");
    $finish;
  end
endmodule
