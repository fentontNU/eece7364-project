close all;
clear all;

%% Initialize variables.
filenames = [{'UlSinrStats.txt'}, {'DlRsrpSinrStats.txt'}];

for k = 1:length(filenames)
    formatSpec = '%f%f%f%f%f%f%[^\n\r]';
    delimiter = '\t';
    startRow = 2;

    full_path = char(strcat(pwd,filesep,filenames(k)));
    %% Open the text file.
    fileID = fopen(full_path,'r');
    dataArray = textscan(fileID, formatSpec, 'Delimiter', delimiter, 'EmptyValue' ,NaN,'HeaderLines' ,startRow-1, 'ReturnOnError', false);
    fclose(fileID);

    time        = dataArray{:, 1};
    cellId      = dataArray{:, 2};
    IMSI        = dataArray{:, 3};
    RNTI        = dataArray{:, 4};
    rsrp        = dataArray{:, 5};
    
    % UL and DL text files have a different amount of headers
    if min(isnan(dataArray{:, 6})) == 1
        sinr_linear = dataArray{:, 5};
    else
        sinr_linear = dataArray{:, 6};
    end
    sinr_db     = log10(sinr_linear);

    %% Clear temporary variables
    clearvars filename delimiter startRow formatSpec fileID dataArray ans;

    %% Plot the Data
    cells = unique(cellId);

    figure(k)
    hold on;
    for i = 1:length(cells)
        index          = bsxfun(@eq,i,cellId);
        time_new{i}    = time(index);
        sinr_db_new{i} = sinr_db(index); 
        legendInfo{i}  = ['Cell ID ' num2str(i)];
        plot(time_new{i},sinr_db_new{i})    
    end
    legend(legendInfo)
    title(filenames(k))
    xlabel('Time (sec)')
    ylabel('SINR (dB)')
    hold off;
end


