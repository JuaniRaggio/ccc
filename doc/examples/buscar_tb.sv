module buscar_tb;
  logic signed [31:0] idx, out;

  buscar dut (.idx(idx), .out(out));

  initial begin
    for (int k = 0; k < 4; k++) begin
      idx = k;
      #1;
      if (out !== (1 << k)) $fatal(1, "buscar(%0d) = %0d, expected %0d", k, out, 1 << k);
      $display("buscar(%0d) = %0d", k, out);
    end
    $display("PASS");
    $finish;
  end
endmodule
