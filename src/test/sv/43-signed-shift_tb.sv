// Arithmetic right shift of a signed value: asr(-32) = -2 (not 268435454).
module asr_tb;
  logic signed [31:0] x, out;
  initial begin
    x = -32; #1;
    if (out !== -2) $fatal(1, "asr(-32) = %0d, expected -2", out);
    x = 48; #1;
    if (out !== 3) $fatal(1, "asr(48) = %0d, expected 3", out);
    $display("PASS");
    $finish;
  end
  asr dut (.x(x), .out(out));
endmodule
