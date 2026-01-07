library ieee;
library machxo2;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
use machxo2.all;

entity HDMIDecoder is	
	port (
		C          : in std_logic;  
		D0         : in std_logic;		
		D1         : in std_logic;
		D2         : in std_logic;
		
		R_BIT      : out std_logic_vector(1 downto 0);
		G_BIT      : out std_logic_vector(1 downto 0);
		B_BIT      : out std_logic_vector(1 downto 0);
		CLK_BIT    : out std_logic;
		
		SCL        : in std_logic;
		SDA        : inout std_logic;
		
		DUMMY      : out std_logic
	);	

	ATTRIBUTE IO_TYPES : string;
	ATTRIBUTE IO_TYPES OF C:  SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF D0: SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF D1: SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF D2: SIGNAL IS "LVDS,-";
end entity;

architecture immediate of HDMIDecoder is

type Tedid is array (0 to 127) of integer;
function make_checksums (edid : in Tedid) return Tedid is
    variable res : Tedid;
	variable s1:integer;
  begin
    s1 := 0;
	for i in 0 to 126 loop
		s1 := (s1 + edid(i)) mod 256;
	end loop;
	res(0 to 126) := edid(0 to 126);
	res(127) := (256 - s1) mod 256;
    return res;
  end;

-- PLL outputs 4 clocks with 45, 90, 135 phase shift
component PLLHDMI is
    port (
        CLKI: in  std_logic;    -- 25.35 MHz
        CLKOP: out  std_logic;  -- 253.5 MHz 
        CLKOS: out  std_logic;  -- 253.5 MHz, 45 degrees shifted
        CLKOS2: out  std_logic; -- 253.5 MHz, 90 degrees shifted
        CLKOS3: out  std_logic  -- 253.5 MHz, 135 degrees shifted
	);
end component;

COMPONENT OSCH
	GENERIC (NOM_FREQ: string);
	PORT (
		STDBY:IN std_logic;
		OSC:OUT std_logic;
		SEDSTDBY:OUT std_logic
	);
END COMPONENT;

component StreamDecoder is	
	port (
		PIXELCLOCK : in std_logic;
		BITCLOCK0  : in std_logic;
		BITCLOCK1  : in std_logic;
		BITCLOCK2  : in std_logic;
		BITCLOCK3  : in std_logic;
		
		STREAM     : in std_logic;  
		
		DE         : out std_logic;
		DATA       : out std_logic_vector(3 downto 0);
		SYNCED     : out std_logic
	);	
end component;

component ram_dual is
	generic
	(
		data_width : integer := 8;
		addr_width : integer := 16;
		numwords   : integer := 2**16
	); 
	port 
	(
		data	: in std_logic_vector(data_width-1 downto 0);
		raddr	: in std_logic_vector(addr_width-1 downto 0);
		waddr	: in std_logic_vector(addr_width-1 downto 0);
		we		: in std_logic := '1';
		rclk	: in std_logic;
		wclk	: in std_logic;
		q		: out std_logic_vector(data_width-1 downto 0)
	);	
end component;


signal BITCLOCK0:std_logic;
signal BITCLOCK1:std_logic;
signal BITCLOCK2:std_logic;
signal BITCLOCK3:std_logic;

signal DE0:std_logic;
signal DE1:std_logic;
signal DE2:std_logic;
signal DATA0:std_logic_vector(3 downto 0);
signal DATA1:std_logic_vector(3 downto 0);
signal DATA2:std_logic_vector(3 downto 0);
signal WADDRESS0:std_logic_vector(11 downto 0);
signal WADDRESS1:std_logic_vector(11 downto 0);
signal WADDRESS2:std_logic_vector(11 downto 0);
signal RADDRESS:std_logic_vector(11 downto 0);
signal RDATA:std_logic_vector(11 downto 0);

signal OSC:std_logic;  
signal SDA_DEGLITCH:std_logic;
signal SCL_DEGLITCH:std_logic;

begin
	pll: PLLHDMI
	PORT MAP ( CLKI => C, CLKOP => BITCLOCK0, CLKOS => BITCLOCK1, CLKOS2 => BITCLOCK2, CLKOS3 => BITCLOCK3 );
	
	OSCInst0: OSCH
	GENERIC MAP( NOM_FREQ => "26.6" )
	PORT MAP ( 
		STDBY=> '0', OSC => OSC,	SEDSTDBY => open 
	);
	
	Dec0:StreamDecoder
	PORT MAP (
		PIXELCLOCK => C,
		BITCLOCK0  => BITCLOCK0, 
		BITCLOCK1  => BITCLOCK1,
		BITCLOCK2  => BITCLOCK2,
		BITCLOCK3  => BITCLOCK3,
		STREAM     => D0,  		
		DE         => DE0,
		DATA       => DATA0,
		SYNCED     => open
	);
	Dec1:StreamDecoder
	PORT MAP (
		PIXELCLOCK => C,
		BITCLOCK0  => BITCLOCK0, 
		BITCLOCK1  => BITCLOCK1,
		BITCLOCK2  => BITCLOCK2,
		BITCLOCK3  => BITCLOCK3,
		STREAM     => D1,  		
		DE         => DE1,
		DATA       => DATA1,
		SYNCED     => open
	);
	Dec2:StreamDecoder
	PORT MAP (
		PIXELCLOCK => C,
		BITCLOCK0  => BITCLOCK0, 
		BITCLOCK1  => BITCLOCK1,
		BITCLOCK2  => BITCLOCK2,
		BITCLOCK3  => BITCLOCK3,
		STREAM     => D2,  		
		DE         => DE2,
		DATA       => DATA2,
		SYNCED     => open
	);
	RAM0: ram_dual generic map(data_width => 4, addr_width => 12, numwords => 4096)
		port map (
			DATA0, 
			RADDRESS,
			WADDRESS0,
			'1',
			C,
			C,
			RDATA(3 downto 0)    -- blue bits
		);
	RAM1: ram_dual generic map(data_width => 4, addr_width => 12, numwords => 4096)
		port map (
			DATA1, 
			RADDRESS,
			WADDRESS1,
			'1',
			C,
			C,
			RDATA(7 downto 4)    -- green bits
		);
	RAM2: ram_dual generic map(data_width => 4, addr_width => 12, numwords => 4096)
		port map (
			DATA2, 
			RADDRESS,
			WADDRESS2,
			'1',
			C,
			C,
			RDATA(11 downto 8)   -- red bits
		);


	-- orchestrate data transfer
	process (C)	
	subtype i13 is integer range 0 to 4095;
	type i13a is array (0 to 2) of i13;
	variable writecounter:i13a;
	variable readcounter:i13;
	variable inactivecounter:i13a;
	variable in_x:i13a;
	variable in_y:i13a;
	constant totaloutwidth:integer := ((960*528)/4) / 256;
	variable out_phase:integer range 0 to 3;
	variable out_x:integer range 0 to 511;
	variable out_y:integer range 0 to 511;
	variable tmpde:std_logic_vector(2 downto 0);
	begin
		if rising_edge(C) then
			-- send output data from the buffer (synchronized to input channel 0)
			if in_x(0)=4 and in_y(0)=0 then
				out_phase := 0;
				out_x := 0;
				out_y := 0;
				readcounter := 0;
			else
				if out_x<340 and out_y<256 and (out_phase=1 or out_phase=2) then
					CLK_BIT <= '1';
				else
					CLK_BIT <= '0';
				end if;	
				if out_x>320 then
					if out_y = 0 then
						R_BIT <= "00";
						G_BIT <= "00";
						B_BIT <= "00";
					else
						R_BIT <= "11";
						G_BIT <= "11";
						B_BIT <= "11";
					end if;
				elsif out_phase=1 then
					R_BIT <= RDATA(11 downto 10);
					G_BIT <= RDATA(7 downto 6);
					B_BIT <= RDATA(3 downto 2);
				elsif out_phase=3 then
					R_BIT <= RDATA(9 downto 8);
					G_BIT <= RDATA(5 downto 4);
					B_BIT <= RDATA(1 downto 0);
				end if;		
				if out_phase<3 then
					out_phase := out_phase+1;
				else
					out_phase:=0;
					if out_x<totaloutwidth-1 then
						out_x := out_x+1;
					else
						out_x := 0;
						if out_y<511 then
							out_y := out_y+1;
						end if;
					end if;
					if out_x<320 and out_y<256 then
						readcounter:=readcounter+1;
					end if;					
				end if;
			end if;
		
			-- monitor changes in the DE values and progress addresses and counters for input
			tmpde := DE2 & DE1 & DE0;
			for i in 0 to 2 loop 
				if tmpde(i)='1' then  -- data available
					if inactivecounter(i)>4000 then    -- end of vertial blanking
						in_x(i) := 0;
						in_y(i) := 0;
					elsif inactivecounter(i)>100 then  -- end of horizontal blanking
						in_x(i) := 0;
						in_y(i) := in_y(i)+1;
					else
						in_x(i) := in_x(i)+1;
						if (in_y(i) mod 2)=0 and in_x(i)>=40 and in_x(i)<680 and (in_x(i) mod 2)=0 then
							writecounter(i):=writecounter(i)+1;
						end if;
					end if;				
					inactivecounter(i) := 0;					
				else
					if inactivecounter(i)>4000 then
						writecounter(i) := 0;
					end if;
					if inactivecounter(i)<4095 then
						inactivecounter(i) := inactivecounter(i)+1;
					end if;
				end if;
			end loop;			
		end if;
		
		WADDRESS0 <= std_logic_vector(to_unsigned(writecounter(0), 12));
		WADDRESS1 <= std_logic_vector(to_unsigned(writecounter(1), 12));
		WADDRESS2 <= std_logic_vector(to_unsigned(writecounter(2), 12));
		RADDRESS <= std_logic_vector(to_unsigned(readcounter, 12));
		
		DUMMY <= '0';
	end process;
	
	
	--process (OSC)
	--variable phase : integer range 0 to 3 := 0;
	--variable x : integer range 0 to 1024 := 0;
	--variable y : integer range 0 to 255 := 0;
	--variable frame : integer range 0 to 255;
	--variable rgb:std_logic_vector(11 downto 0);
	--begin
		--if rising_edge(OSC) then
			---- generate picture
			--rgb := "000000000000";
			--if x<320 and y<256 then
				--if x<256 then
					--rgb(11 downto 8) := std_logic_vector(to_unsigned(x/16, 4));
					--rgb(7 downto 4)  := std_logic_vector(to_unsigned(y/16, 4));
				--else
					--rgb(11 downto 8) := std_logic_vector(to_unsigned( (x+frame)mod 16, 4));
					--rgb(7 downto 4)  := std_logic_vector(to_unsigned( (x+frame)mod 16, 4));
					--rgb(3 downto 0)  := std_logic_vector(to_unsigned( (x+frame)mod 16, 4));
				--end if;
			--end if;	
			---- generate output signals
			--if x<320 then
				--if phase<=1 then
----					CLK_BIT <= '1';
					--R_BIT <= rgb(11 downto 10);
					--G_BIT <= rgb(7 downto 6);
					--B_BIT <= rgb(3 downto 2);
				--else 
----					CLK_BIT <= '0';
					--R_BIT <= rgb(9 downto 8);
					--G_BIT <= rgb(5 downto 4);
					--B_BIT <= rgb(1 downto 0);
				--end if;			
			--else
				--if x<340 and phase<=1 then
----					CLK_BIT <= '1';
				--else
----					CLK_BIT <= '0';
				--end if;
				--if y=0 then
					--R_BIT <= "00";
					--G_BIT <= "00";
					--B_BIT <= "00";
				--else
					--R_BIT <= "11";
					--G_BIT <= "11";
					--B_BIT <= "11";
				--end if;
			--end if;
			---- progress counters
			--if phase<3 then
				--phase:=phase+1;
			--else
				--phase := 0;
				--if x<530-1 then
					--x := x+1;
				--else 
					--x := 0;
					--if y<256-1 then
						--y := y+1;
					--else 
						--y := 0;
						--frame := (frame+1) mod 256;
					--end if;
				--end if;
			--end if;
		--end if;
	--end process;



	-- de-glitch the SDA
	process (OSC)
	variable locktime:integer range 0 to 15 := 0;
	variable value:std_logic := '1';
	variable newvalue:std_logic := '1';	
	begin
		if rising_edge(OSC) then
			if value/=newvalue then
				if locktime>0 then
					locktime:=locktime-1;
				else
					value:=newvalue;
					locktime:=15;
				end if;
			end if;
			newvalue := SDA;
		end if;
		SDA_DEGLITCH <= value;
	end process;
	-- de-glitch the SCL
	process (OSC)
	variable locktime:integer range 0 to 15 := 0;
	variable value:std_logic := '1';
	variable newvalue:std_logic := '1';
	begin
		if rising_edge(OSC) then
			if value/=newvalue then
				if locktime>0 then
					locktime:=locktime-1;
				else
					value:=newvalue;
					locktime:=15;
				end if;
			end if;
			newvalue := SCL;
		end if;
		SCL_DEGLITCH <= value;
	end process;
	
    -- send EDID
	process (SCL_DEGLITCH,SDA_DEGLITCH,OSC)
	constant pixelclock:integer := 25350000/10000;
	constant hvisible:integer := 720;
	constant hfront:integer := 64;
	constant hsync:integer := 64;
	constant hback:integer := 112;
	constant hblanking:integer := hfront+hsync+hback;
	constant hsize_mm:integer := 1080;
	constant vvisible:integer := 512;
	constant vfront:integer := 4;
	constant vsync:integer := 4;
	constant vback:integer := 8;
	constant vblanking:integer := vfront+vsync+vback;
	constant vsize_mm:integer := 768;
	constant edid_without_sums:Tedid := (
		16#00#,16#ff#,16#ff#,16#ff#,16#ff#,16#ff#,16#ff#,16#00#,                                                                        -- standard header
		16#0d#,16#f0#,16#01#,16#00#,16#01#,16#00#,16#00#,16#00#,16#0a#,16#22#,                                                          -- manufacturer info
		16#01#,16#03#,                                                                                                                    -- EDID version
		16#a0#,                                                                                                                           -- video input parameters
		16#28#,16#1e#,                                                                                                                    -- screen size in cm
		16#78#,                                                                                                                           -- gamma factor
		16#06#,                                                                                                                           -- supported features bitmap    
		16#EE#,16#91#,16#A3#,16#54#,16#4C#,16#99#,16#26#,16#0F#,16#50#,16#54#,                                                          -- chromaticity coordinates 
		16#00#,16#00#,16#00#,                                                                                                             -- established modes
		16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,               -- standard modes
		-- detailed timing descriptor
		pixelclock mod 256,                                                      -- 0
		pixelclock/256,                                                          -- 1
		hvisible mod 256,                                                        -- 2
		hblanking mod 256,                                                       -- 3
		16*(hvisible/256) + (hblanking/256),                                    -- 4
		vvisible mod 256,                                                        -- 5
		vblanking mod 256,                                                       -- 6
		16*(vvisible/256) + (vblanking/256),                                    -- 7
		hfront mod 256,                                                          -- 8
		hsync mod 256,                                                           -- 9
		16*(vfront mod 16) + (vsync mod 16),                                    -- 10
		64*(hfront/256) + 16*(hsync/256) + 4*(vfront/16) + (vsync/16),         -- 11
		hsize_mm mod 256,                                                        -- 12
		vsize_mm mod 256,                                                        -- 13
		16*(hsize_mm/256) + (vsize_mm/256),                                     -- 14
		0,                                                                       -- 15
		0,                                                                       -- 16
		2#00011000#,                                                             -- 17
		16#00#,16#00#,16#00#,16#fc#,16#00#,16#4C#,16#45#,16#44#,16#4d#,16#41#,16#54#,16#52#,16#49#,16#58#,16#0a#,16#20#,16#20#,16#20#, -- monitor information
		16#00#,16#00#,16#00#,16#10#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#, -- dummy
		16#00#,16#00#,16#00#,16#10#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#, -- dummy
		16#00#,                                                                                                                            -- number of extension blocks
		16#00#                                                                                                                             -- place for checksum
	);
	constant edid:Tedid := make_checksums(edid_without_sums);
	type Tstate is (idle, address, ackaddr, transfer, acktrans);
	variable sda_history:std_logic_vector(15 downto 0) := "1111111111111111";
	variable scl_history:std_logic_vector(15 downto 0) := "1111111111111111";
	variable detect_start:boolean := false;
	variable state: Tstate := idle;
	variable addrbuffer:std_logic_vector(6 downto 0);
	variable writebuffer:std_logic_vector(7 downto 0);
	variable readbuffer:std_logic_vector(7 downto 0);
	variable bitcount:integer range 0 to 7;
	variable readmode:boolean;
	variable index:integer range 0 to 255 := 0;
	variable isfirstbyte:boolean;
	variable out_sda:std_logic := '1';
	begin
		if rising_edge(OSC) then
			if sda_history="1111111100000000" then
				detect_start := scl_history="1111111111111111";
			elsif scl_history = "0000000011111111" then
				detect_start := false;
			end if;
			sda_history := sda_history(14 downto 0) & SDA_DEGLITCH;
			scl_history := scl_history(14 downto 0) & SCL_DEGLITCH;
		end if;
	
		if rising_edge(SCL_DEGLITCH) then
			out_sda := '1';
			if detect_start then   
				state := address;
				addrbuffer := "000000" & SDA_DEGLITCH;
				bitcount := 1;
			else
				-- state machine
				case state is
				when address =>
					if bitcount<7 then
						addrbuffer := addrbuffer(5 downto 0) & SDA_DEGLITCH;
						bitcount := bitcount+1;
					elsif addrbuffer = "1010000" then
						readmode := SDA_DEGLITCH='1';
						isfirstbyte := true;
						state := ackaddr;
						out_sda := '0';   -- ack after matching address
					else 
						state := idle;
					end if;
				when ackaddr =>
					if readmode then
						readbuffer := std_logic_vector(to_unsigned(edid(index), 8));
						out_sda := readbuffer(7);
						readbuffer := readbuffer(6 downto 0) & '0';
					end if;
					bitcount := 0;
					isfirstbyte := true;
					state := transfer;
				when transfer =>
					if bitcount<7 then
						bitcount := bitcount+1;
						if readmode then
							out_sda := readbuffer(7);
							readbuffer := readbuffer(6 downto 0) & '0';
						else
							writebuffer := writebuffer(6 downto 0) & SDA_DEGLITCH;
						end if;
					else
						if readmode then -- master is reading with index auto-increment
							index := index+1;
						else  -- master is writing to slave
							if isfirstbyte then 
								index := to_integer(unsigned(writebuffer & SDA_DEGLITCH));
							end if;
							out_sda := '0';  -- ack after each byte
						end if;
						isfirstbyte := false;
						state := acktrans;
					end if;				
				when acktrans =>
					if readmode then
						if SDA_DEGLITCH='1' then	-- host did not ack the data - that means end of transmission
							state := idle;
						else
							readbuffer := std_logic_vector(to_unsigned(edid(index), 8));
							out_sda := readbuffer(7);
							readbuffer := readbuffer(6 downto 0) & '0';
						end if;
					end if;
					bitcount := 0;
					state := transfer;
				when others =>
				end case;			
			end if;
		end if;
		if falling_edge(SCL_DEGLITCH) then
			if out_sda='0' then
				SDA <= '0';
			else
				SDA <= 'Z';
			end if;
		end if;
	end process;

end immediate;
