% first line: <#servers> <#ToRdownlinks> <#ToRuplinks> <#ToRs>
% second line: <#slices> <time_in_slice_type_0> <time_in_slice_type_1> ...
% next #slices lines: tors that each uplink is connected to (-1 means disconnected)
%   (***tors indexed from zero)
% rest of lines: paths between ToRs
%   Format: <slice number>
%           <srcToR> <dstToR> <intermediate_queue0> <intermediate_queue1> ...
%           <srcToR> <dstToR> <intermediate_queue0> <intermediate_queue1> ...
%           ...
%           <next slice number>
%           ...
%   (***slices, ToRs, queues indexed from zero)
%   (***queue indexing starts from the first downlink (0) to the last
%   downlink (#downlinks-1) to the first uplink (#downlinks) to the last
%   uplink (#downlinks+#uplinks-1). The queues we record here will always
%   correspond to uplinks, so their indexing will start at (#downlinks))

// host-0   host-1   host-2   host-3   host-4   host-5   host-6   host-7
// 7 2 0 3, 4 6 3 5, 2 0 4 7, 3 4 1 0, 1 3 2 6, 6 5 7 1, 5 1 6 4, 0 7 5 2
// 7 4 0 3, 4 2 3 5, 2 1 4 7, 3 5 1 0, 1 0 2 6, 6 3 7 1, 5 7 6 4, 0 6 5 2
// 7 4 1 3, 4 2 0 5, 2 1 5 7, 3 5 6 0, 1 0 7 6, 6 3 2 1, 5 7 3 4, 0 6 4 2
// 7 4 1 6, 4 2 0 7, 2 1 5 3, 3 5 6 2, 1 0 7 5, 6 3 2 4, 5 7 3 0, 0 6 4 1
// 5 4 1 6, 1 2 0 7, 6 1 5 3, 7 5 6 2, 4 0 7 5, 0 3 2 4, 2 7 3 0, 3 6 4 1
// 5 2 1 6, 1 6 0 7, 6 0 5 3, 7 4 6 2, 4 3 7 5, 0 5 2 4, 2 1 3 0, 3 7 4 1
// 5 2 0 6, 1 6 3 7, 6 0 4 3, 7 4 1 2, 4 3 2 5, 0 5 7 4, 2 1 6 0, 3 7 5 1
// 5 2 0 3, 1 6 3 5, 6 0 4 7, 7 4 1 0, 4 3 2 6, 0 5 7 1, 2 1 6 4, 3 7 5 2
