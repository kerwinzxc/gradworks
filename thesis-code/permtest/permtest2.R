suppressMessages(require(gtools))
suppressMessages(require(ggplot2))

combinations <- function(n, r, v = 1:n, set = TRUE, repeats.allowed=FALSE)
{
  if(mode(n) != "numeric" || length(n) != 1
     || n < 1 || (n %% 1) != 0) stop("bad value of n")
  if(mode(r) != "numeric" || length(r) != 1
     || r < 1 || (r %% 1) != 0) stop("bad value of r")
  if(!is.atomic(v) || length(v) < n)
    stop("v is either non-atomic or too short")
  if( (r > n) & repeats.allowed==FALSE)
    stop("r > n and repeats.allowed=FALSE")
  if(set) {
    v <- unique(sort(v))
    if (length(v) < n) stop("too few different elements")
  }
  v0 <- vector(mode(v), 0)
  ## Inner workhorse
  if(repeats.allowed)
    sub <- function(n, r, v)
    {
      if(r == 0) v0 else
        if(r == 1) matrix(v, n, 1) else
          if(n == 1) matrix(v, 1, r) else
            rbind( cbind(v[1], Recall(n, r-1, v)),
                   Recall(n-1, r, v[-1]))
    }
  else
    sub <- function(n, r, v)
    {
      if(r == 0) v0 else
        if(r == 1) matrix(v, n, 1) else
          if(r == n) matrix(v, 1, n) else
            rbind(cbind(v[1], Recall(n-1, r-1, v[-1])),
                  Recall(n-1, r, v[-1]))
    }
  sub(n, r, v[1:n])
}

permhist = function(o2, pvalues, observed, ppvalue, count, total) {
  pdf(o2,width=8,height=6)
  hist(pvalues, main=sprintf("Permutation Histogram (p=%5.4f, n=%d, N=%d)", ppvalue, count, total), xlab = "mean", xlim=c(0,.02))
  par(new=TRUE)   
  abline(v=observed, col="blue", lty=2)
  par(new=TRUE)   
  plot(density(pvalues), col=2, yaxt="n", xaxt="n", bty='n', xlab="", ylab="", main='', xlim=c(0,.02))
  dev.off()
}



# read f1 and f2 that contains thickness measurements for each group
# perform combined t-test
permtest = function(f1,f2) {
  data1.input_df = read.table(f1)[,-1]
  data2.input_df = read.table(f2)[,-1]
  
  data1 = as.matrix(data1.input_df)
  data2 = as.matrix(data2.input_df)
  
  combined = cbind(data1,data2)

  len1 = ncol(data1)
  len2 = ncol(combined)
  
  indexes = t(combinations(len2,len1))
  indexes = indexes[,1:(ncol(indexes)/2)]
  diff.pvalues = NULL
  for (i in 1:ncol(indexes)) {
#  for (i in 1:3) {
    data1.random = combined[,indexes[,i]]
    data2.random = combined[,-indexes[,i]]
    diff.pvalues[i] = t.test(c(data2.random), c(data1.random))$p.value
#     data = cbind(data1.random,data2.random)
#     write.table(data1.random, sprintf("/Prime/Thesis-Data/Rodent-Thickness/RPV_Thickness/RPV1+3/RPV3_AIE_vs_RPV3_Control_Right/Statistical/Permtest/dataA_%03d.txt", i,row.names=FALSE, col.names=FALSE))
#     write.table(data2.random, sprintf("/Prime/Thesis-Data/Rodent-Thickness/RPV_Thickness/RPV1+3/RPV3_AIE_vs_RPV3_Control_Right/Statistical/Permtest/dataB_%03d.txt", i,row.names=FALSE, col.names=FALSE))
  }  

  write.table(diff.pvalues, "/Prime/Thesis-Data/Rodent-Thickness/RPV_Thickness/RPV1+3/RPV3_AIE_vs_RPV3_Control_Right/Statistical/data_initialDenseSampling_PermTest.txt", row.names=FALSE, col.names=FALSE)

}

permtest("/Prime/Thesis-Data/Rodent-Thickness/RPV_Thickness/RPV1+3/RPV3_AIE_vs_RPV3_Control_Right/Statistical/data_initialDenseSampling_RPV3C.txt", "/Prime/Thesis-Data/Rodent-Thickness/RPV_Thickness/RPV1+3/RPV3_AIE_vs_RPV3_Control_Right/Statistical/data_initialDenseSampling_RPV3E.txt")

# args <- commandArgs(trailingOnly = TRUE)
# if (length(args) >= 4) {
#   dir = args[1]
#   data1_prefix = args[2]
#   data2_prefix = args[3]
#   output_prefix = args[4]
#   
#   if (!file.exists(dir)) {
#     stop(sprintf("'%s' not exists", dir))
#   }
# } else {
#   stop(sprintf("permtest.R root-dir data1-prefix data2-prefix output-prefix"))
# }
#dir = "/Prime/Rodent-Thickness-Data/RPV_Thickness/RPV1+3/RPV3_AIE_vs_RPV3_Control_Right"

# for (j in 1:20) {
#   region_number = j
#   data1_file = sprintf("%s/Statistical/%s%02d.txt", dir, data1_prefix, region_number)
#   data2_file = sprintf("%s/Statistical/%s%02d.txt", dir, data2_prefix, region_number)
#   data_out = sprintf("%s/Statistical/%s%02d.txt", dir, output_prefix, region_number)
#   pdf_out = sprintf("%s/Statistical/%s%02d.pdf", dir, output_prefix, region_number)
#   if (!file.exists(data1_file)) {
#     cat(sprintf("'%s' not exists", data1_file))
#     break
#   } 
#   if (!file.exists(data2_file)) {
#     cat(sprintf("'%s' not exists", data2_file))
#     break
#   }
#   diff = permtest(data1_file, data2_file, data_out, pdf_out)
# }